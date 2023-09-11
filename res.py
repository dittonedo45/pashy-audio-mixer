import fobject
import sys
import asyncio
import random
import aiofiles
import typing

class Pac(typing.List):
    def append(self, *args):
        if (len(self)>5000):
            del self[:90]
        else:
            super().append(*args)

async def run(*args):
        loop=asyncio.get_running_loop ()
        return await loop.run_in_executor(None, *args)

def track_threading (arg):
    for track in map (fobject.Format, sys.argv[1:]):
        while True:
            try:
                ret=track.process_audio ()
                if not isinstance(ret, tuple):
                    yield ret
            except RuntimeError:
                break

class Format(fobject.Format):
    def __init__(s, *a, **kw):
        s.args=a
        (fobject.Format).__init__(s, *a, *kw)
        pass
    async def __aiter__(self):
        while True:
            try:
                ret=self.process_audio ()
                if not isinstance(ret, tuple):
                    yield ret
                else:
                    print(ret, file=sys.stderr)
            except RuntimeError:
                break

def args():
    yield from iter(sys.argv[1:])
    pass

async def rand(l):
    loop=asyncio.get_running_loop()
    async def randint(*x):
        return await loop.run_in_executor (None, random.randint, *x)
    s=l[await randint(0,len(l)-1)]
    return s

async def Deck(tracks):
    while True:
        x=Format(await rand(tracks))
        async for i in x:
            yield x, i

class Filter(fobject.Filter):
    pass

do_not_just_change=None
main_filter=Filter("[in1] lowpass, [in2]amerge, asetrate=44100*1.2[out]")

async def deck1(x, index):
    global do_not_just_change
    std=aiofiles.stdout.buffer
    async for x, i in Deck(x):
        await asyncio.sleep (0)
        main_filter.process_audio (i, index)
        while True:
            try:
                ret=main_filter.process_audio (None, index)
                if not ret:
                    continue
                sys.stdout.buffer.write (ret)
            except StopIteration:
                break


async def filter_switch():
    global main_filter
    global do_not_just_change
    i=0
    while True:
        await asyncio.sleep(19)
        if (i%2)==0:
            for j in range(100,300, 100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in1] lowpass,
                        [in2]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in2] anullsink;
                [in1]asetrate=44100*1.{j}[out]""")
            await asyncio.sleep(80)
            for j in range(100,300, -100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in1] lowpass,
                        [in2]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in2] anullsink;
                [in1]asetrate=44100*1.{j}[out]""")
        else:
            for j in range(100,300, 100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in2] lowpass,
                        [in1]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in1] anullsink;
                [in2]asetrate=44100*1.{j}[out]""")
            await asyncio.sleep(80)
            for j in range(100,300, -100):
                async with do_not_just_change:
                    main_filter=fobject.Filter(f"""[in2] lowpass,
                        [in1]amerge, asetrate=44100*1.{j}[out]""")
                await asyncio.sleep(0.8)
            main_filter=fobject.Filter(f"""[in1] anullsink;
                [in2]asetrate=44100*1.{j}[out]""")
        i=i+1

async def main (cb, *args):
    global do_not_just_change
    do_not_just_change=asyncio.Semaphore(2)
    await asyncio.gather(
        *map(lambda x: deck1(list(args), x), range(2)),
        filter_switch()
        )

asyncio.run (main(*args()))
