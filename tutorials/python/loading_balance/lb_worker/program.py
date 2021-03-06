
from piworker import PiWorker
from consts import piConst
from spa.clientside import CConnectionContext, CSocketPool
import sys

cc = CConnectionContext("localhost", 20901, "lb_worker", "pwdForlbworker")
with CSocketPool(PiWorker) as spPi:
    ok = spPi.StartSocketPool(cc, 1)
    print('Press ENTER key to shutdown the demo application ......')
    line = sys.stdin.readline()