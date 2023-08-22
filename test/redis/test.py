import time
import pandas as pd
import subprocess
from pathlib import Path
import os
import numpy as np


ROOT_DIR = Path(os.path.dirname(__file__))

LOCAL = (ROOT_DIR / '..' / '..' / 'local').resolve()

print(LOCAL)



def run_trials(binary, preload='',  waiting_time=10, with_defrag=True, env={}, config='redis.conf'):
    environ = os.environ.copy()
    environ['LD_PRELOAD'] = str(LOCAL / 'lib' / 'librsstracker.so') + ' ' + preload
    if not with_defrag:
        environ['ANCH_NO_DEFRAG'] = '1'

    for k in env:
        environ[k] = str(env[k])

    results = []
    print('starting...')
    os.system('rm -f dump.rdb rss.trace')

    # env['ALASKA_LOG'] = f'alaska_{trial}.log'
    redis_cmd = subprocess.Popen([binary, ROOT_DIR / config], env=environ, shell=False)
    time.sleep(.1) # Wait for the server to start

    os.system(f'cat {ROOT_DIR / "fragmentation.redis"} | redis-cli')
    time.sleep(waiting_time)

    print('ending...')
    redis_cmd.kill()
    print('joining...')
    redis_cmd.wait()

    df = pd.read_csv('rss.trace', names=['time_ms', 'rss'])
    os.system('rm -f dump.rdb rss.trace')
    results.append(df)

    return pd.concat(results)

# for lb in np.arange(1.0, 2, 0.5):
#     # for oh in np.arange(0.05, 0.2, 0.05):
#     for aggro in np.arange(0.1, 1, 0.1):
#         res = run_trials(ROOT_DIR / 'build/redis-server', env={
#             'FRAG_LB': lb,
#             # 'ANCH_TARG_OVERHEAD': oh,
#             'ANCH_AGGRO': aggro})
#         res.to_csv(f'alaska-lb{lb}-aggro{aggro}.csv', index=False)

res = run_trials('/home/nick/dev/redis/src/redis-server', with_defrag=False, config='ad.conf')
res.to_csv('baseline.csv', index=False)
#
res = run_trials(ROOT_DIR / 'build/redis-server')
res.to_csv('anchorage.csv', index=False)
