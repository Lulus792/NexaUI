"""Fresh private registry for the full original task; reuse SDK bus helpers."""
import argparse,os,select,subprocess,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tests'))
from run_atspi_native import rpc,stop

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--registry',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args();command=args.command
    if command and command[0]=='--':command=command[1:]
    if not command:parser.error('native driver command required')
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=True);private=root/'ax-bus';private.mkdir(mode=0o700)
    address='unix:path='+str(private/'bus');assert len(os.fsencode(private/'bus'))<108
    env=dict(os.environ,GSETTINGS_BACKEND='memory',NEXAUI_EDITOR_TEST_ATSPI_ADDRESS=address,PYTHONDONTWRITEBYTECODE='1')
    env.pop('NO_AT_BRIDGE',None)
    registry=None;bus=None
    with (private/'bus.log').open('xb') as bus_log,(private/'registry.log').open('xb') as registry_log:
        try:
            bus=subprocess.Popen(['dbus-daemon','--session','--nofork','--address='+address,'--print-address=1'],
                env=env,stdout=subprocess.PIPE,stderr=bus_log,text=True,start_new_session=True)
            if not select.select([bus.stdout],[],[],10)[0]:raise RuntimeError('private editor bus startup timed out')
            actual=bus.stdout.readline().strip();assert actual.startswith(address) and bus.poll() is None,actual
            registry_env=dict(env,AT_SPI_BUS_ADDRESS=address,DBUS_SESSION_BUS_ADDRESS=address,XDG_RUNTIME_DIR=str(private))
            registry=subprocess.Popen([str(args.registry.resolve(strict=True))],env=registry_env,stdout=registry_log,stderr=subprocess.STDOUT,start_new_session=True)
            deadline=time.monotonic()+5
            while True:
                reply=rpc(address,'org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus.NameHasOwner','string:org.a11y.atspi.Registry')
                if reply.returncode==0 and 'boolean true' in reply.stdout:break
                assert registry.poll() is None and bus.poll() is None and time.monotonic()<deadline,'private Editor registry startup'
                time.sleep(.01)
            return subprocess.run(command,env=env).returncode
        finally:
            stop(registry);stop(bus)
if __name__=='__main__':raise SystemExit(main())
