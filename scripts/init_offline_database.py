"""Create a dedicated LOCAL PostgreSQL database. No remote connection is made."""
from pathlib import Path
import argparse, base64, ctypes, getpass, json, os, secrets, subprocess, sys
from ctypes import wintypes

ROOT=Path(__file__).resolve().parent.parent
PSQL=Path(r'D:\postgresql\bin\psql.exe')

class Blob(ctypes.Structure):
    _fields_=[('cbData',wintypes.DWORD),('pbData',ctypes.POINTER(ctypes.c_ubyte))]

def protect(value, decrypt=False):
    data=value if isinstance(value,bytes) else value.encode('utf-8')
    buffer=ctypes.create_string_buffer(data)
    source=Blob(len(data),ctypes.cast(buffer,ctypes.POINTER(ctypes.c_ubyte)))
    target=Blob()
    if decrypt:
        ok=ctypes.windll.crypt32.CryptUnprotectData(ctypes.byref(source),None,None,None,None,1,ctypes.byref(target))
    else:
        ok=ctypes.windll.crypt32.CryptProtectData(ctypes.byref(source),'TransNet offline database',None,None,None,1,ctypes.byref(target))
    if not ok: raise ctypes.WinError()
    try: return ctypes.string_at(target.pbData,target.cbData)
    finally: ctypes.windll.kernel32.LocalFree(target.pbData)

def quote(value): return "'"+str(value).replace("'","''")+"'"

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,default=5432)
    parser.add_argument('--admin-user',default='postgres')
    parser.add_argument('--password-env')
    parser.add_argument('--config-dir',type=Path,default=ROOT/'offline/config')
    parser.add_argument('--no-pause',action='store_true')
    args=parser.parse_args()
    print('TransNet Pro Offline: initialize LOCAL database')
    print(f'Local server: 127.0.0.1:{args.port}; database: transnetpro_offline; application role: tnp_offline')
    admin_password=os.environ[args.password_env] if args.password_env else getpass.getpass('LOCAL PostgreSQL postgres administrator password (hidden): ')
    def sql(query,db='postgres',user=None,password=None):
        env=os.environ.copy();env.update(PGPASSWORD=admin_password if password is None else password,PGCONNECT_TIMEOUT='5',PGCLIENTENCODING='UTF8')
        process=subprocess.run([str(PSQL),'-X','-w','-q','-A','-t','-v','ON_ERROR_STOP=1','-h','127.0.0.1','-p',str(args.port),'-U',user or args.admin_user,'-d',db],
                               input=query.encode('utf-8'),capture_output=True,env=env,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
        if process.returncode:
            error=process.stderr.decode('utf-8',errors='replace')
            for value in (admin_password,password):
                if value: error=error.replace(value,'[REDACTED]')
            raise RuntimeError(error.strip())
        return process.stdout.decode('utf-8',errors='replace').strip()
    sql('SELECT 1;')
    config_path=args.config_dir/'database.json'
    fallback=ROOT.parent/'tnpsource_developmentKit/transnetproCode/offline/config/database.json'
    credential_path=config_path if config_path.exists() else fallback
    old=json.loads(credential_path.read_text(encoding='utf-8-sig')) if credential_path.exists() else None
    role_exists=sql("SELECT 1 FROM pg_roles WHERE rolname='tnp_offline';")
    if role_exists:
        if not old or old.get('port')!=args.port:
            raise RuntimeError('tnp_offline already exists without matching application credentials; refusing to reset its password.')
        app_password=protect(base64.b64decode(old['password_dpapi']),decrypt=True).decode('utf-8')
    else:
        app_password=secrets.token_urlsafe(32)
        sql(f"CREATE ROLE tnp_offline LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE PASSWORD {quote(app_password)};")
    owner=sql("SELECT pg_get_userbyid(datdba) FROM pg_database WHERE datname='transnetpro_offline';")
    if owner and owner!='tnp_offline':
        raise RuntimeError('transnetpro_offline exists with another owner; refusing to modify it.')
    if not owner: sql('CREATE DATABASE transnetpro_offline OWNER tnp_offline;')
    # Persist the dedicated role credential before schema migration so retries keep the same identity.
    args.config_dir.mkdir(parents=True,exist_ok=True)
    config={'driver':'QPSQL','host':'127.0.0.1','port':args.port,'database':'transnetpro_offline','user':'tnp_offline',
            'schema':'tnp2_data','password_dpapi':base64.b64encode(protect(app_password)).decode('ascii')}
    temp=config_path.with_suffix('.tmp');temp.write_text(json.dumps(config,ensure_ascii=False,indent=2),encoding='utf-8');os.replace(temp,config_path)
    sql('CREATE EXTENSION IF NOT EXISTS postgis;','transnetpro_offline')
    sql('BEGIN;\n'+(ROOT/'src/offline/schema.sql').read_text(encoding='utf-8-sig')+'\nCOMMIT;','transnetpro_offline','tnp_offline',app_password)
    count=sql("SELECT count(*) FROM information_schema.tables WHERE table_schema='tnp2_data';",'transnetpro_offline','tnp_offline',app_password)
    result={'configured':True,'host':'127.0.0.1','port':args.port,'database':'transnetpro_offline','role':'tnp_offline','tables':int(count),'remote_access':False}
    (args.config_dir/'setup_result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result,indent=2));print('SUCCESS. Dedicated local database is ready. You may start the offline application.')
    return 0

if __name__=='__main__':
    status=0
    try: status=main()
    except (Exception,KeyboardInterrupt) as error:
        print(f'ERROR: {error}',file=sys.stderr);status=1
    if '--no-pause' not in sys.argv:
        try: input('Press Enter to close this window...')
        except (EOFError,KeyboardInterrupt): pass
    raise SystemExit(status)
