import os
import subprocess
import sys
import time
import configparser
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import concurrent.futures
import threading
import logging
from logging.handlers import TimedRotatingFileHandler


def enumerateDir(dir_path, ext, log):
    log.info('enumerateDir %s %s', dir_path, ext)
    res = []
    try:
        for path in os.listdir(dir_path):
            fullPath = os.path.join(dir_path, path)
            if os.path.isfile(fullPath):
                split = os.path.splitext(path)
                if split[1] == '.'+ext:
                    res.append(split[0])
    except Exception as e:
        log.error(str(e))
    return res

def opusEncode(wavFile, opusPath, log):
    log.info('Encode %s',wavFile)
    fileName = os.path.splitext(os.path.basename(wavFile))[0]
    opusFile = os.path.join(opusPath, fileName)+'.opus'
    subprocess.run(['/usr/bin/opusenc', wavFile, opusFile, '--quiet'])
    log.info('Encoded %s', opusFile)

def flacEncode(wavFile, flacPath, log):
    fileName = os.path.splitext(os.path.basename(wavFile))[0]
    flacFile = os.path.join(flacPath, fileName)+'.flac'
    log.info('Encode %s to %s',wavFile, flacFile)
    result = subprocess.run(['/usr/bin/flac', wavFile, '-o', flacFile, '-s'])
    log.info('result %d %s', result.returncode, result.args)
    if result.returncode == 0:
        log.info('Encoded %s', flacFile)
    else:
        log.info('Failed to encode %s ', result.args)


class Logger():
    def __init__(self, name, configPath, log):
        log.info('Logger %s created', name)
        self.name = name

        config = configparser.ConfigParser()
        config.read(os.path.join(configPath, name)+'.ini')
        if 'path' in config:
            self.wavPath = os.path.join(config['path'].get('audio', fallback='.'), 'wav', name)
            self.opusPath = os.path.join(config['path'].get('audio', fallback='.'), 'opus', name)
            self.flacPath = os.path.join(config['path'].get('audio', fallback='.'), 'flac', name)
            self.opusEncode = (config['keep'].getint('opus', fallback=0) > 0)
            self.flacEncode = (config['keep'].getint('flac', fallback=0) > 0)
            self.log = log
            self.log.info('Logger %s opus=%d flac=%d', name, self.opusEncode, self.flacEncode)

            self.opusEncodeList = []
            self.flacEncodeList = []
            if self.opusEncode == True:
                self.createEncodeDir(self.opusPath)
            if self.flacEncode == True:
                self.createEncodeDir(self.flacPath)
            self.createEncodeLists()
            self.ok = True
        else:
            log.warning('Failed to read config file for %s', name)
            self.ok = False

    def createEncodeDir(self, path):
        try:
            os.makedirs(path)
        except FileExistsError:
            self.log.debug('Dir %s already exists', path)


    def createEncodeLists(self):
        if self.opusEncode == True:
            self.opusEncodeList = self.createEncodeList(self.opusPath, 'opus')
        if self.flacEncode == True:
            self.flacEncodeList = self.createEncodeList(self.flacPath, 'flac')

    def createEncodeList(self, path, ext):
        self.log.info('Logger %s workout wav files that still need encoding', self.name)
        wavList = enumerateDir(self.wavPath, 'wav', self.log)
        opusSet = set(enumerateDir(path, ext, self.log))
        # look through the opus directory and remove any file already encoded
        encodeList = [file for file in wavList if file not in opusSet]
        self.log.info('Logger %s has %d files to still encode', self.name, len(encodeList))
        return encodeList

        
    def encode(self, executor):
        for wavFile in self.opusEncodeList:
            wavFile = wavFile + '.wav'
            executor.submit(opusEncode, os.path.join(self.wavPath, wavFile), self.opusPath, self.log)
        for wavFile in self.flacEncodeList:
            wavFile = wavFile + '.wav'
            executor.submit(flacEncode, os.path.join(self.wavPath, wavFile), self.flacPath, self.log)
        self.opusEncodeList = []
        self.flacEncodeList = []



class CreatedHandler(FileSystemEventHandler):
    def __init__(self, executor, loggerDict, log):
        self.executor = executor
        self.loggerDict = loggerDict
        self.log = log

    def on_created(self, event):
        self.log.debug('File %s created',event.src_path)
    def on_closed(self, event):
        self.log.info('File %s closed',event.src_path)
        #work out the opus path
        theDir = os.path.split(event.src_path)[0]
        components = theDir.split('/')
        if len(components) != 0:
            loggerName = components[-1]
        else:
            loggerName = theDir 

        loggerObj = self.loggerDict.get(loggerName)
        if loggerObj != None:
            if loggerObj.opusEncode != 0:
                self.executor.submit(opusEncode, event.src_path, loggerObj.opusPath, self.log)
            if loggerObj.flacEncode != 0:
                self.executor.submit(flacEncode, event.src_path, loggerObj.flacPath, self.log)
        else:
            self.log.debug('logger %s not defined', loggerName)

class LoggerHandler(FileSystemEventHandler):
    def __init__(self, executor, loggerDict, loggerPath, log):
        self.executor = executor
        self.loggerDict = loggerDict
        self.loggerPath = loggerPath
        self.log = log

    def on_created(self, event):
        loggerName = os.path.basename(event.src_path)
        self.log.info("%s created", loggerName)
        loggerObj = Logger(loggerName, self.loggerPath, self.log)
        if loggerObj.ok == True:
            self.loggerDict[loggerName] = loggerObj

    def on_deleted(self, event):
        loggerName = os.path.basename(event.src_path)
        self.log.info("%s removed", loggerName)
        try:
            self.loggerDict.pop(loggerName)
        except:
            self.log.debug('%s removed but never configured', loggerName)
        
def logNamer(default_name):
    base_filename,ext,date = default_name.split('.')
    return f'{date}.{ext}'

def main():
    if len(sys.argv) < 2:
        print('Usage: encoder.py [full path to config file]')
        quit()
    else:
        run()

def run():  
    config = configparser.ConfigParser()
    config.read(sys.argv[1])
    if ('path' in config) == False:
        print('Could not read config file')
        quit();

    logging.basicConfig(level=logging.INFO, format='%(asctime)s\t%(levelname)s\t%(message)s')
    log = logging.getLogger('encoder')
    log.setLevel(logging.INFO)
    ## Here we define our formatter
    formatter = logging.Formatter('%(asctime)s\t%(levelname)s\t%(message)s')
    logHandler = TimedRotatingFileHandler(os.path.join(config['path'].get('log', fallback='/var/log/encoder'), 'enc.log'), when='h', interval=1, utc=True, backupCount=24)
    logHandler.setLevel(logging.INFO)
    logHandler.setFormatter(formatter)
    logHandler.namer = logNamer
    log.addHandler(logHandler)


    loggerDict = {}
    # find all the possible loggers, create the loggers and work out what files need encoding    
    for loggerName in enumerateDir(config['path'].get('loggers', fallback='.'), 'ini', log):
        loggerObj = Logger(loggerName, config['path'].get('loggers', fallback='.'), log)
        if loggerObj.ok == True:
            loggerDict[loggerName] = loggerObj

    #create the thread pool that will launch the encocders
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=len(loggerDict)+4)
    
    # start a recursive observer for the wav file directory to watch for new wav files
    observer=Observer()
    event_handler = CreatedHandler(executor, loggerDict, log)
    logger_handler = LoggerHandler(executor, loggerDict, config['path'].get('loggers', fallback='.'), log)

    try:
        observer.schedule(event_handler, path=config['path'].get('wav', fallback='.'), recursive=True)
        observer.schedule(logger_handler, path=config['path'].get('loggers', fallback='.'))
        observer.start()
    
        for loggerName in loggerDict:
            loggerDict[loggerName].encode(executor)

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            log.warning('KeyboardInterrupt - stop')
            observer.stop()

        observer.join()
        log.warning('Encoder exiting')
    except FileNotFoundError:
        log.critical('Path %s does not exist', config['path'].get('wav', fallback='.'))
        quit()


if __name__ == '__main__':
    main()
