import redis
import string
import random
import time
import threading
import subprocess
from pathlib import Path
import os


ROOT_DIR = Path(os.path.dirname(__file__))

results = []

def track():
    global last
    cur = now()
    if cur > last:
        last = cur
        rss, frag = get_rss()
        results.append((cur - start_time, rss))
        print(f'{cur - start_time},{rss}')
        # print(f'{rss},{frag}')


for trial in range(10):
    print('starting...')
    os.system('rm -f dump.rdb')
    env = os.environ.copy()
    env['ALASKA_LOG'] = f'activedefrag_{trial}.log'
    env['LD_PRELOAD'] = ROOT_DIR / '../../local/lib/libalaska.so'
    # redis_cmd = subprocess.Popen([ROOT_DIR / 'build/redis-server.base', ROOT_DIR / 'redis.conf'], env=env)
    redis_cmd = subprocess.Popen(['/home/nick/dev/redis/src/redis-server', ROOT_DIR / 'redis.conf'], env=env)
    time.sleep(.5) # Wait for the server to start

    os.system(f'cat {ROOT_DIR / "fragmentation.redis"} | redis-cli')
    # wait 10 seconds to let defrag do it's thing
    time.sleep(10)

    print('ending...')
    redis_cmd.kill()
    print('joining...')
    redis_cmd.wait()


# print('time,rss')
# for time, rss in results:
#     print(f'{time},{rss}')
