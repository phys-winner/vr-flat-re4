#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>

typedef intptr_t ssize_t;
#define S_ISDIR(mode) (((mode)&_S_IFMT)==_S_IFDIR)
enum fault { NORMAL, SHORT_AND_INTR, OPEN_FAIL, WRITE_FAIL, WRITE_ZERO, SYNC_FAIL, CLOSE_FAIL, CLOSE_INTR, RENAME_FAIL };
static enum fault fault;
static int opens, writes, syncs, closes, renames, alive, logs;
static int test_lstat(const char* path,struct stat* info) {
    struct _stat64i32 native;
    int result = _stat(path,&native);
    if (!result) { memset(info,0,sizeof *info); info->st_mode = native.st_mode; }
    if (!result && (GetFileAttributesA(path)&FILE_ATTRIBUTE_REPARSE_POINT)) info->st_mode = 0;
    return result;
}
static int test_mkdir(const char* path,int mode) { (void)mode; return _mkdir(path); }
static int test_mkstemp(char* path) {
    opens++;
    if (fault == OPEN_FAIL) { errno = EACCES; return -1; }
    if (fault == SHORT_AND_INTR && opens == 1) {
        memcpy(path+strlen(path)-6,"RTRY00",6); errno = EINTR; return -1;
    }
    assert(!strcmp(path+strlen(path)-6,"XXXXXX"));
    errno_t error = _mktemp_s(path,strlen(path)+1);
    if (error) { errno = error; return -1; }
    int fd = _open(path,_O_WRONLY|_O_BINARY|_O_CREAT|_O_EXCL,_S_IREAD|_S_IWRITE);
    if (fd >= 0) alive++;
    return fd;
}
static ssize_t test_write(int fd,const void* data,size_t size) {
    writes++;
    if (fault == SHORT_AND_INTR && writes == 1) { errno = EINTR; return -1; }
    if (fault == WRITE_FAIL && writes > 1) { errno = ENOSPC; return -1; }
    if (fault == WRITE_ZERO && writes > 1) return 0;
    if (size > 4096) size = 4096;
    return _write(fd,data,(unsigned)size);
}
static int test_fsync(int fd) {
    syncs++;
    if (fault == SHORT_AND_INTR && syncs == 1) { errno = EINTR; return -1; }
    if (fault == SYNC_FAIL) { errno = EIO; return -1; }
    return _commit(fd);
}
static int test_close(int fd) {
    closes++; int result = _close(fd); alive--;
    if (fault == CLOSE_FAIL || fault == CLOSE_INTR) { errno = fault == CLOSE_INTR ? EINTR : EIO; return -1; }
    return result;
}
static int test_rename(const char* from,const char* to) {
    renames++;
    if (fault == SHORT_AND_INTR && renames == 1) { errno = EINTR; return -1; }
    if (fault == RENAME_FAIL) { errno = EACCES; return -1; }
    if (!MoveFileExA(from,to,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { errno = EIO; return -1; }
    return 0;
}
static int test_unlink(const char* path) { return _unlink(path); }
#define lstat test_lstat
#define mkdir test_mkdir
#define mkstemp test_mkstemp
#define write test_write
#define fsync test_fsync
#define close test_close
#define rename test_rename
#define unlink test_unlink
#define LOGF(...) ((void)logs++)
#include "../re4fix_mobile_text.h"

static int jni_pending,jni_get,jni_release,jni_fail_get;
static const char* jni_path="/not-an-app-root";
static jsize jni_length=16;
static jboolean jni_exception(JNIEnv* env){(void)env;return jni_pending?JNI_TRUE:JNI_FALSE;}
static jsize jni_size(JNIEnv* env,jstring root){(void)env;assert(root==(jstring)1);return jni_length;}
static const char* jni_chars(JNIEnv* env,jstring root,jboolean* copy){(void)env;(void)copy;assert(root==(jstring)1);++jni_get;if(jni_fail_get){jni_pending=1;return 0;}return jni_path;}
static void jni_release_chars(JNIEnv* env,jstring root,const char* chars){(void)env;assert(root==(jstring)1&&chars==jni_path);++jni_release;}
static void check_external_root_jni(void){
    assert(mobile_text_api_root_valid("/storage/emulated/0/Android/data/com.Armature.VR4/files"));
    assert(mobile_text_api_root_valid("/storage/emulated/10/Android/data/com.Armature.VR4/files"));
    assert(mobile_text_api_root_valid("/storage/1234-ABCD/Android/data/com.Armature.VR4/files"));
    assert(!mobile_text_api_root_valid(0)&&!mobile_text_api_root_valid("relative/Android/data/com.Armature.VR4/files"));
    assert(!mobile_text_api_root_valid("/storage/emulated/0/../Android/data/com.Armature.VR4/files"));
    assert(!mobile_text_api_root_valid("/storage/emulated/0/Android/data/other.package/files"));
    assert(!mobile_text_api_root_valid("/storage/emulated/0/Android/data/com.Armature.VR4/files/"));
    struct JNINativeInterface functions={0};functions.ExceptionCheck=jni_exception;functions.GetStringUTFLength=jni_size;functions.GetStringUTFChars=jni_chars;functions.ReleaseStringUTFChars=jni_release_chars;JNIEnv env=&functions;
    assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(0,0,(jstring)1));
    assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(&env,0,0));
    jni_pending=1;assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(&env,0,(jstring)1)&&jni_pending&&jni_get==0);jni_pending=0;
    jni_length=1000;assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(&env,0,(jstring)1)&&jni_get==0);jni_length=16;
    assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(&env,0,(jstring)1)&&jni_get==1&&jni_release==1);
    jni_fail_get=1;assert(!Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(&env,0,(jstring)1)&&jni_get==2&&jni_release==1&&jni_pending);jni_fail_get=0;jni_pending=0;
    assert(opens==0&&writes==0);
}

static const unsigned char old[] = "previous verified PAK";
static void reset(enum fault next) { fault = next; opens = writes = syncs = closes = renames = alive = logs = 0; }
static void write_old(const char* target) {
    FILE* stream = fopen(target,"wb"); assert(stream);
    assert(fwrite(old,1,sizeof old,stream) == sizeof old); assert(!fclose(stream));
}
static void assert_bytes(const char* path,const unsigned char* expected,size_t size) {
    FILE* stream = fopen(path,"rb"); assert(stream);
    unsigned char buffer[4096];
    for (size_t index = 0; index < size;) {
        size_t chunk = size-index < sizeof buffer ? size-index : sizeof buffer;
        assert(fread(buffer,1,chunk,stream) == chunk); assert(!memcmp(buffer,expected+index,chunk)); index += chunk;
    }
    assert(fgetc(stream) == EOF); assert(!fclose(stream));
}
static void assert_no_temp(const char* directory) {
    char pattern[1024]; snprintf(pattern,sizeof pattern,"%s/*.tmp.*",directory);
    WIN32_FIND_DATAA result; HANDLE find = FindFirstFileA(pattern,&result);
    assert(find == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND);
}
int main(int argc,char** argv) {
    check_external_root_jni();
    assert(argc == 2);
    if (argc == 999) deploy_mobile_text();
    char target[1024],directory[1024],missing[1024],obstruction[1024];
    snprintf(directory,sizeof directory,"%s/UE4Game/VR4/VR4/Saved/Paks",argv[1]);
    snprintf(target,sizeof target,"%s/%s",directory,MOBILE_TEXT_PAK_NAME);
    reset(NORMAL); assert(!mobile_text_deploy_at(argv[1]));
    assert_bytes(target,g_mobile_text_pak,sizeof g_mobile_text_pak);
    assert(!alive && closes == 1 && renames == 1 && logs == 1); assert_no_temp(directory);
    write_old(target); reset(SHORT_AND_INTR); assert(!mobile_text_deploy_at(argv[1]));
    assert_bytes(target,g_mobile_text_pak,sizeof g_mobile_text_pak);
    assert(opens == 2 && writes > 2 && syncs == 2 && closes == 1 && renames == 2 && !alive);
    assert_no_temp(directory);
    const enum fault failures[] = {OPEN_FAIL,WRITE_FAIL,WRITE_ZERO,SYNC_FAIL,CLOSE_FAIL,CLOSE_INTR,RENAME_FAIL};
    const int errors[] = {EACCES,ENOSPC,EIO,EIO,EIO,EINTR,EACCES};
    for (unsigned i = 0; i < sizeof failures/sizeof failures[0]; i++) {
        write_old(target); reset(failures[i]);
        assert(mobile_text_deploy_at(argv[1]) == -1 && errno == errors[i]);
        assert_bytes(target,old,sizeof old); assert_no_temp(directory);
        assert(!alive && logs == 1 && closes == (failures[i] == OPEN_FAIL ? 0 : 1));
        assert(renames == (failures[i] == RENAME_FAIL ? 1 : 0));
    }
    reset(NORMAL); assert(mobile_text_deploy_at(0) == -1 && errno == EINVAL);
    assert(mobile_text_deploy_at("") == -1 && errno == EINVAL);
    assert(mobile_text_deploy_at("/files") == -1 && errno == EINVAL);
    char long_path[1025]; memset(long_path,'x',sizeof long_path-1); long_path[sizeof long_path-1] = 0;
    assert(mobile_text_deploy_at(long_path) == -1); assert_bytes(target,old,sizeof old);
    snprintf(missing,sizeof missing,"%s/nonexistent/files",argv[1]);
    assert(mobile_text_deploy_at(missing) == -1 && errno == ENOENT);
    snprintf(obstruction,sizeof obstruction,"%s/blocked",argv[1]); assert(!_mkdir(obstruction));
    snprintf(missing,sizeof missing,"%s/files",obstruction); assert(!_mkdir(missing));
    snprintf(obstruction,sizeof obstruction,"%s/UE4Game",missing);
    FILE* stream = fopen(obstruction,"wb"); assert(stream); assert(!fclose(stream));
    assert(mobile_text_deploy_at(missing) == -1 && errno == ENOTDIR); assert_bytes(target,old,sizeof old);
    puts("PASS: actual mobile text C IO, exact embedded bytes, short writes/EINTR, atomic replacement, old PAK preserved on open/write/fsync/close/rename failures, no duplicate close, temp cleanup and fixed-prefix directory guards");
    return 0;
}
