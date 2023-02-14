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
                if split[1] == '.'+ext:
                    res.append((fullPath, os.path.getmtime(fullPath)))
    except Exception as e:
        log.error(str(e))
    return res


class Logger():
    def __init__(self, name, configPath, log):
        log.info('Logger %s created', name)
        self.name = name

        config = configparser.ConfigParser()
        config.read(os.path.join(configPath, name)+'.ini')

        self.wavPath = os.path.join(config['path'].get('audio', '.'), 'wav', name)
        self.opusPath = os.path.join(config['path'].get('audio', '.'), 'opus', name)
        self.flacPath = os.path.join(config['path'].get('audio', '.'), 'flac', name)

        self.opusAge = config['keep'].get('opus', 0)*3600
        self.flacAge = config['keep'].get('flac', 0)*3600
        self.wavAge = config['keep'].get('wav', 0)*3600
        self.log = log

    def removeAllFiles(self):
        self.removeFiles(self.wavAge, self.wavPath, 'wav')
        self.removeFiles(self.opusAge, self.opusPath, 'opus')
        self.removeFiles(self.flacAge, self.flacPath, 'flac')

    def removeFiles(self, age, path, ext):
        removeTp = time.time()-age
        files = enumerateDir(path, ext, self.log)
        count = 0
        for fileTp in files:
            if fileTp[1] < removeTp and os.path.exists(fileTp[0]):
                os.remove(fileTp[0])
                count += 1
        self.log.info('Removed %d files from %s', count, path)




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
        quit();
    
    logging.basicConfig(level=logging.INFO, format='%(asctime)s\t%(levelname)s\t%(message)s')
    log = logging.getLogger('cleanup')
    log.setLevel(logging.INFO)
    ## Here we define our formatter
    formatter = logging.Formatter('%(asctime)s\t%(levelname)s\t%(message)s')
    logHandler = TimedRotatingFileHandler(os.path.join(config['path'].get('log', '/var/log/cleanup'), 'cleanup.log'), when='h', interval=1, utc=True)
    logHandler.setLevel(logging.INFO)
    
    ## Here we set our logHandler's formatter
    logHandler.setFormatter(formatter)
    log.addHandler(logHandler)

    # find all the possible loggers, create the loggers and work out what files need encoding    
    for loggerName in enumerateDir(config['path'].get('loggers', '.'), 'ini'):
        loggerObj = Logger(loggerName, config['path'].get('loggers', '.'), log)
        loggerObj.RemoveAllFiles()

    log.info('Finished')


if __name__ == '__main__':
    main()