import redis
import string
import random
import time

# this benchmark allocates 700,000 random keys of length 240
# then, inserts 170,000 random keys of length 492
# This operation is the same thing that is outlined in the MESH paper

r = redis.Redis(host='localhost', port=6379, db=0)


def get_rss():
    temp = redis.Redis(host='localhost', port=6379, db=0)
    info = temp.info('memory')

    return info['used_memory_rss'], info['mem_fragmentation_ratio']


def gen_value(length):
    # length = random.randrange(64, length)
    return ''.join(random.choices(string.ascii_uppercase + string.digits, k=length))

def now():
    return int(time.time())

scale = 10000 # 10000


start_time = now()
last = start_time

def track():
    global last
    cur = now()
    if cur > last:
        last = cur
        rss, frag = get_rss()
        # print(f'{cur - start_time},{get_rss()}')
        print(f'{rss},{frag}')

# print('first round...')


for i in range(70 * scale):
    track()
    v = gen_value(240)
    r.set(gen_value(128), v)



# print('second round...')

for i in range(17 * scale):
    track()
    v = gen_value(1028)
    r.set(gen_value(128), v)
