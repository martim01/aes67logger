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



def enumerateDir(dir_path, ext):
    res = []
    try:
        for path in os.listdir(dir_path):
            fullPath = os.path.join(dir_path, path)
            if os.path.isfile(fullPath):
                split = os.path.splitext(path)[1]
                if split[1] == ext:
                    res.append(split[0])
    except Exception as e:
        print(str(e))
    return res

def opusEncode(wavFile, opusPath):
    logging.info("Encode %s",wavFile)
    fileName = os.path.splitext(os.path.basename(wavFile))[0]
    opusFile = os.path.join(opusPath, fileName)
    subprocess.run(["opusenc", wavFile, opusFile])
    logging.info("Encoded %s", opusFile)

class Logger():
    def __init__(self, name, wavPath, opusPath):
        logging.info('Logger %s created', name)
        self.name = name
        self.wavPath = wavPath+"/"+name
        self.opusPath = opusPath+"/"+name
        self.encodeList = []

    def createEncodeList(self):
        wavList = enumerateDir(self.wavPath, "wav", True)
        opusSet = set(enumerateDir(self.opusPath, "opus", True))
        # look through the opus directory and remove any file already encoded
        self.encodeList = [file for file in wavList if file not in opusSet]
        logging.info('Logger %s has %d files to still encode', self.name, len(self.encodeList))

    def encode(self, executor):
        for wavFile in self.encodeList:
            executor.submit(opusEncode, wavFile, self.opusPath)

class CreatedHandler(FileSystemEventHandler):
    def __init__(self, executor, opusPath):
        self.executor = executor
        self.opusPath = opusPath

    def on_created(self, event):
        logging.info("File %s created",event.src_path)
    def on_closed(self, event):
        logging.info("File %s closed",event.src_path)
        #work out the opus path
        dir = os.path.split(event.src_path)[0]
        components = dir.split('/')
        if len(components) != 0:
            loggerName = components[-1]
        else:
            loggerName = dir 
        self.executor.submit(opusEncode, event.src_path, os.path.join(self.opusPath, loggerName))



def main():
    if len(sys.argv) < 2:
        print("Usage: encoder.py [full path to config file]")
        quit()
    else:
        run()

def run():

    logging.basicConfig(format='%(asctime)s %(message)s')

    config = configparser.ConfigParser()
    config.read(sys.argv[1])
    if ("paths" in config) == False:
        logging.critical("Could not read config file")
        quit();

    loggers = []
    # find all the possible loggers, create the loggers and work out what files need encoding    
    for logger in enumerateDir(config["path"].get("loggers", "."), "ini", True):
        loggers[logger] = Logger(logger, config["path"].get("wav", "."), config["path"].get("opus", "."))
        loggers[logger].createEncodeList()

    #create the thread pool that will launch the encocders
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=len(loggers)+4)
    
    # start a recursive observer for the wav file directory to watch for new wav files
    observer=Observer()
    event_handler = CreatedHandler(executor, config["path"].get("opus", "."))
    observer.schedule(event_handler, path=config["path"].get("wav", "."), recursive=True)
    observer.start()

    for logger in loggers:
        logger.encode(executor)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        logging.warning("KeyboardInterrupt - stop")
        observer.stop()

    observer.join()
    logging.info("Encoder exiting")

if __name__ == "__main__":
    main()