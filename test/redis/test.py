import redis
import string
import random
import time
import threading
import pandas as pd
import subprocess
from pathlib import Path
import os


ROOT_DIR = Path(os.path.dirname(__file__))

LOCAL = (ROOT_DIR / '..' / '..' / 'local').resolve()

print(LOCAL)



def run_trials(binary, preload='', trials=1, frag_count=4, waiting_time=4, with_defrag=True):
    results = []
    for trial in range(trials):
        print('starting...')
        os.system('rm -f dump.rdb rss.trace')
        env = os.environ.copy()
        env['LD_PRELOAD'] = str(LOCAL / 'lib' / 'librsstracker.so') + ' ' + preload
        if not with_defrag:
            env['ANCH_NO_DEFRAG'] = '1'
        # env['ALASKA_LOG'] = f'alaska_{trial}.log'
        redis_cmd = subprocess.Popen([binary, ROOT_DIR / 'redis.conf'], env=env, shell=False)
        time.sleep(.3) # Wait for the server to start

        for i in range(frag_count):
            os.system(f'cat {ROOT_DIR / "fragmentation.redis"} | redis-cli')
            # wait for defrag to do it's thing
            time.sleep(waiting_time)


        print('ending...')
        redis_cmd.kill()
        print('joining...')
        redis_cmd.wait()

        df = pd.read_csv('rss.trace', names=['time_ms', 'rss'])
        os.system('rm -f dump.rdb rss.trace')
        results.append(df)

    return pd.concat(results)


res = run_trials(ROOT_DIR / 'build/redis-server')
res.to_csv('alaska.csv', index=False)

res = run_trials(ROOT_DIR / 'build/redis-server', with_defrag=False)
res.to_csv('nodefrag.csv', index=False)

res = run_trials(ROOT_DIR / 'build/redis-server.base', with_defrag=False)
res.to_csv('baseline.csv', index=False)
