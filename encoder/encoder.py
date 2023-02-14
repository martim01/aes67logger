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
    logging.warning("enumerateDir %s %s", dir_path, ext)
    res = []
    try:
        for path in os.listdir(dir_path):
            fullPath = os.path.join(dir_path, path)
            if os.path.isfile(fullPath):
                split = os.path.splitext(path)
                if split[1] == "."+ext:
                    res.append(split[0])
    except Exception as e:
        logging.error(str(e))
    return res

def opusEncode(wavFile, opusPath):
    logging.warning("Encode %s",wavFile)
    fileName = os.path.splitext(os.path.basename(wavFile))[0]
    opusFile = os.path.join(opusPath, fileName)
    subprocess.run(["opusenc", wavFile, opusFile, '--quiet'])
    logging.warning("Encoded %s", opusFile)

class Logger():
    def __init__(self, name, wavPath, opusPath):
        logging.warning('Logger %s created', name)
        self.name = name
        self.wavPath = wavPath+"/"+name
        self.opusPath = opusPath+"/"+name
        self.encodeList = []
        try:
            os.makedirs(self.opusPath)
        except FileExistsError:
            logging.warning('Dir already exists')

    def createEncodeList(self):
        logging.warning('Logger %s workout wav files that still need encoding', self.name)
        wavList = enumerateDir(self.wavPath, "wav")
        opusSet = set(enumerateDir(self.opusPath, "opus"))
        # look through the opus directory and remove any file already encoded
        self.encodeList = [file for file in wavList if file not in opusSet]
        logging.warning('Logger %s has %d files to still encode', self.name, len(self.encodeList))

    def encode(self, executor):
        for wavFile in self.encodeList:
            wavFile = wavFile + ".wav"
            executor.submit(opusEncode, os.path.join(self.wavPath, wavFile), self.opusPath)
        self.encodeList = []

class CreatedHandler(FileSystemEventHandler):
    def __init__(self, executor, opusPath):
        self.executor = executor
        self.opusPath = opusPath

    def on_created(self, event):
        logging.warning("File %s created",event.src_path)
    def on_closed(self, event):
        logging.warning("File %s closed",event.src_path)
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
    for logger in enumerateDir(config["paths"].get("loggers", "."), "ini"):
        loggerObj = Logger(logger, config["paths"].get("wav", "."), config["paths"].get("opus", "."))
        loggerObj.createEncodeList()
        loggers.append(loggerObj)

    #create the thread pool that will launch the encocders
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=len(loggers)+4)
    
    # start a recursive observer for the wav file directory to watch for new wav files
    observer=Observer()
    event_handler = CreatedHandler(executor, config["paths"].get("opus", "."))
    try:
        observer.schedule(event_handler, path=config["paths"].get("wav", "."), recursive=True)
        observer.start()
    
        for loggerObj in loggers:
            loggerObj.encode(executor)

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logging.warning("KeyboardInterrupt - stop")
            observer.stop()

        observer.join()
        logging.warning("Encoder exiting")
    except FileNotFoundError:
        logging.critical("Path %s does not exist", config["paths"].get("wav", "."))
        quit();


if __name__ == "__main__":
    main()
