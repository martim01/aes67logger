import configparser
import sys
import os
import time
import logging
from logging.handlers import TimedRotatingFileHandler

def enumerateDir(dir_path, ext, log):
    res = []
    try:
        for path in os.listdir(dir_path):
            fullPath = os.path.join(dir_path, path)
            if os.path.isfile(fullPath):
                split = os.path.splitext(path)
                if ext == '*' or split[1] == '.'+ext:
                    res.append((fullPath, os.path.getmtime(fullPath)))
    except Exception as e:
        log.error(str(e))
    return res

def removeFiles(age, path, ext, log):
        if age > 0:
            removeTp = time.time()-age
            files = enumerateDir(path, ext, log)
            count = 0
            for fileTp in files:
                if fileTp[1] < removeTp and os.path.exists(fileTp[0]):
                    try:
                        os.remove(fileTp[0])
                        count += 1
                    except OSError as e:
                        log.warning('Could not remove file %s %s', fileTp[0], e.strerror)
            log.info('Removed %d files from %s', count, path)


class Logger():
    def __init__(self, name, configPath, log):

        self.name = os.path.splitext(os.path.split(name)[1])[0]
        log.info('Logger %s created', self.name)

        config = configparser.ConfigParser()
        config.read(name)
        if "path" in config:
            self.wavPath = config['path'].get('audio', fallback='.')+ '/wav/'+self.name
            self.opusPath = config['path'].get('audio',fallback='.')+'/opus/'+self.name
            self.flacPath = config['path'].get('audio', fallback='.')+'/flac/'+self.name
        else:
            log.error('Logger %s failed to read config file', name)

        self.opusAge = config['keep'].getint('opus', fallback=0)*3600
        self.flacAge = config['keep'].getint('flac', fallback=0)*3600
        self.wavAge = config['keep'].getint('wav', fallback=0)*3600
        self.log = log
        self.log.info('%s wavPath=%s',self.name, self.wavPath)
        self.log.info('%s opusPath=%s',self.name, self.opusPath)
        self.log.info('%s flacPath=%s',self.name, self.flacPath)

    def removeAllFiles(self):
        removeFiles(self.wavAge, self.wavPath, 'wav', self.log)
        removeFiles(self.opusAge, self.opusPath, 'opus', self.log)
        removeFiles(self.flacAge, self.flacPath, 'flac', self.log)

    



def main():
    if len(sys.argv) < 2:
        print('Usage: cleanup.py [full path to encoder config file]')
        quit()
    else:
        run()

def run():
    config = configparser.ConfigParser()
    config.read(sys.argv[1])
    if ('path' in config) == False:
        print('Could not read config file')
        quit()
    
    logging.basicConfig(level=logging.INFO, format='%(asctime)s\t%(levelname)s\t%(message)s')
    log = logging.getLogger('cleanup')
    log.setLevel(logging.INFO)
    ## Here we define our formatter
    formatter = logging.Formatter('%(asctime)s\t%(levelname)s\t%(message)s')
    logHandler = TimedRotatingFileHandler(os.path.join(config['path'].get('log', fallback='/var/log/encoder'), 'cleanup.log'), when='h', interval=1, utc=True)
    logHandler.setLevel(logging.INFO)
    
    ## Here we set our logHandler's formatter
    logHandler.setFormatter(formatter)
    log.addHandler(logHandler)

    # find all the possible loggers, create the loggers and work out what files need deleting
    for loggerTuple in enumerateDir(config['path'].get('loggers', fallback='.'), 'ini',log):
        loggerObj = Logger(loggerTuple[0], config['path'].get('loggers', fallback='.'), log)
        loggerObj.removeAllFiles()

    # remove all files from /tmp
    removeFiles(3600, '/tmp', '*', log)

    log.info('Finished')


if __name__ == '__main__':
    main()
