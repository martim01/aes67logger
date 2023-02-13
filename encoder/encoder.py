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
    log.info("enumerateDir %s %s", dir_path, ext)
    res = []
    try:
        for path in os.listdir(dir_path):
            fullPath = os.path.join(dir_path, path)
            if os.path.isfile(fullPath):
                split = os.path.splitext(path)
                if split[1] == "."+ext:
                    res.append(split[0])
    except Exception as e:
        log.error(str(e))
    return res

def opusEncode(wavFile, opusPath, log):
    log.info("Encode %s",wavFile)
    fileName = os.path.splitext(os.path.basename(wavFile))[0]
    opusFile = os.path.join(opusPath, fileName)+".opus"
    subprocess.run(["opusenc", wavFile, opusFile, '--quiet'])
    log.info("Encoded %s", opusFile)

class Logger():
    def __init__(self, name, wavPath, opusPath, log):
        log.info('Logger %s created', name)
        self.name = name
        self.wavPath = wavPath+"/"+name
        self.opusPath = opusPath+"/"+name
        self.encodeList = []
        self.log = log
        try:
            os.makedirs(self.opusPath)
        except FileExistsError:
            log.debug('Dir already exists')

    def createEncodeList(self):
        self.log.info('Logger %s workout wav files that still need encoding', self.name)
        wavList = enumerateDir(self.wavPath, "wav", self.log)
        opusSet = set(enumerateDir(self.opusPath, "opus", self.log))
        # look through the opus directory and remove any file already encoded
        self.encodeList = [file for file in wavList if file not in opusSet]
        self.log.info('Logger %s has %d files to still encode', self.name, len(self.encodeList))

    def encode(self, executor):
        for wavFile in self.encodeList:
            wavFile = wavFile + ".wav"
            executor.submit(opusEncode, os.path.join(self.wavPath, wavFile), self.opusPath, self.log)
        self.encodeList = []

class CreatedHandler(FileSystemEventHandler):
    def __init__(self, executor, opusPath, log):
        self.executor = executor
        self.opusPath = opusPath
        self.log = log

    def on_created(self, event):
        self.log.debug("File %s created",event.src_path)
    def on_closed(self, event):
        self.log.info("File %s closed",event.src_path)
        #work out the opus path
        dir = os.path.split(event.src_path)[0]
        components = dir.split('/')
        if len(components) != 0:
            loggerName = components[-1]
        else:
            loggerName = dir 
        self.executor.submit(opusEncode, event.src_path, os.path.join(self.opusPath, loggerName), self.log)


def logNamer(default_name):
    base_filename,ext,date = default_name.split('.')
    return f"{date}.{ext}"

def main():
    if len(sys.argv) < 2:
        print("Usage: encoder.py [full path to config file]")
        quit()
    else:
        run()

def run():  
    config = configparser.ConfigParser()
    config.read(sys.argv[1])
    if ("paths" in config) == False:
        print("Could not read config file")
        quit();

    logging.basicConfig(level=logging.INFO, format='%(asctime)s\t%(levelname)s\t%(message)s')
    log = logging.getLogger('encoder')
    log.setLevel(logging.INFO)
    ## Here we define our formatter
    formatter = logging.Formatter('%(asctime)s\t%(levelname)s\t%(message)s')
    logHandler = logging.handlers.TimedRotatingFileHandler(os.path.join(config["paths"].get("log", "/var/log/encoder"), "enc.log"), when='h', interval=1, utc=True)
    logHandler.setLevel(logging.INFO)
    
    ## Here we set our logHandler's formatter
    logHandler.setFormatter(formatter)
    logHandler.namer = logNamer
    log.addHandler(logHandler)
    log.info("TEST")

    loggers = []
    # find all the possible loggers, create the loggers and work out what files need encoding    
    for logger in enumerateDir(config["paths"].get("loggers", "."), "ini", log):
        loggerObj = Logger(logger, config["paths"].get("wav", "."), config["paths"].get("opus", "."), log)
        loggerObj.createEncodeList()
        loggers.append(loggerObj)

    #create the thread pool that will launch the encocders
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=len(loggers)+4)
    
    # start a recursive observer for the wav file directory to watch for new wav files
    observer=Observer()
    event_handler = CreatedHandler(executor, config["paths"].get("opus", "."), log)
    try:
        observer.schedule(event_handler, path=config["paths"].get("wav", "."), recursive=True)
        observer.start()
    
        for loggerObj in loggers:
            loggerObj.encode(executor)

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            log.warning("KeyboardInterrupt - stop")
            observer.stop()

        observer.join()
        log.warning("Encoder exiting")
    except FileNotFoundError:
        log.critical("Path %s does not exist", config["paths"].get("wav", "."))
        quit();


if __name__ == "__main__":
    main()
