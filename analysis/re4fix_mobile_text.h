// Deploy before native Pak initialization; Saved/Paks is an authored auto-discovery folder.
#include <errno.h>
#include <sys/stat.h>
#include <jni.h>
#include "mobile_text_pak.h"

#define MOBILE_TEXT_PAK_NAME "pakchunk99-Android_ETC2_999_P.pak"
static int mobile_text_directory(const char* path, int create) {
    struct stat info;
    int result;
    do { result = lstat(path,&info); } while (result < 0 && errno == EINTR);
    if (!result) {
        if (S_ISDIR(info.st_mode)) return 0;
        errno = ENOTDIR; return -1;
    }
    if (!create || errno != ENOENT) return -1;
    do { result = mkdir(path,0700); } while (result < 0 && errno == EINTR);
    if (result < 0 && errno != EEXIST) return -1;
    do { result = lstat(path,&info); } while (result < 0 && errno == EINTR);
    if (result < 0) return -1;
    if (!S_ISDIR(info.st_mode)) { errno = ENOTDIR; return -1; }
    return 0;
}
static int mobile_text_deploy_at(const char* files_path) {
    static const char* const parts[] = {"UE4Game","VR4","VR4","Saved","Paks"};
    char directory[1024] = "", destination[1024] = "", temporary[1024] = "";
    const char* stage = "files parent";
    int fd = -1, error;
    size_t length = files_path ? strlen(files_path) : 0;
    if (!length || length >= sizeof directory) { errno = EINVAL; goto fail; }
    memcpy(directory,files_path,length+1);
    char* slash = strrchr(directory,'/');
    if (!slash || slash == directory || !slash[1]) { errno = EINVAL; goto fail; }
    *slash = 0;
    if (mobile_text_directory(directory,0)) goto fail;
    *slash = '/';
    stage = "files directory";
    if (mobile_text_directory(directory,1)) goto fail;
    for (unsigned i = 0; i < sizeof parts/sizeof parts[0]; i++) {
        stage = "Saved/Paks directory";
        size_t part = strlen(parts[i]);
        if (length+part+2 > sizeof directory) { errno = ENAMETOOLONG; goto fail; }
        directory[length++] = '/';
        memcpy(directory+length,parts[i],part+1); length += part;
        if (mobile_text_directory(directory,1)) goto fail;
    }
    stage = "Pak filename";
    int n = snprintf(destination,sizeof destination,"%s/%s",directory,MOBILE_TEXT_PAK_NAME);
    if (n < 0 || (size_t)n >= sizeof destination) { errno = ENAMETOOLONG; goto fail; }
    n = snprintf(temporary,sizeof temporary,"%s.tmp.XXXXXX",destination);
    if (n < 0 || (size_t)n >= sizeof temporary) { temporary[0] = 0; errno = ENAMETOOLONG; goto fail; }
    stage = "temporary open";
    do {
        // mkstemp may change its template even when interrupted.
        snprintf(temporary,sizeof temporary,"%s.tmp.XXXXXX",destination);
        fd = mkstemp(temporary);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) { temporary[0] = 0; goto fail; }
    stage = "temporary write";
    for (size_t written = 0; written < sizeof g_mobile_text_pak;) {
        ssize_t count = write(fd,g_mobile_text_pak+written,sizeof g_mobile_text_pak-written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { if (!count) errno = EIO; goto fail; }
        written += (size_t)count;
    }
    stage = "temporary fsync";
    do { n = fsync(fd); } while (n < 0 && errno == EINTR);
    if (n < 0) goto fail;
    stage = "temporary close";
    n = close(fd); fd = -1; // Linux consumes the descriptor even on EINTR; never close it twice.
    if (n < 0) goto fail;
    stage = "atomic rename";
    do { n = rename(temporary,destination); } while (n < 0 && errno == EINTR);
    if (n < 0) goto fail;
    LOGF("MOBILETEXT deployed %zu bytes to %s",sizeof g_mobile_text_pak,destination);
    return 0;
fail:
    error = errno;
    if (fd >= 0) close(fd);
    if (temporary[0]) {
        do { n = unlink(temporary); } while (n < 0 && errno == EINTR);
    }
    LOGF("MOBILETEXT deploy failed: %s path='%s' errno=%d (%s)",stage,directory,error,strerror(error));
    errno = error;
    return -1;
}
static void deploy_mobile_text(void) {
    mobile_text_deploy_at("/sdcard/Android/data/com.Armature.VR4/files");
}

static int mobile_text_api_root_valid(const char* path) {
    static const char suffix[]="/Android/data/com.Armature.VR4/files";
    if (!path || path[0]!='/' || strstr(path,"//") || strstr(path,"/../") || strstr(path,"/./")) return 0;
    size_t length=strlen(path),ending=sizeof suffix-1;
    if (length<=ending || length>=1000 || memcmp(path+length-ending,suffix,ending)) return 0;
    for (size_t index=0;index<length;++index) if ((unsigned char)path[index]<32 || (unsigned char)path[index]>=127) return 0;
    return 1;
}

JNIEXPORT jboolean JNICALL Java_com_epicgames_ue4_MobileAndroid_nativeExternalRoot(JNIEnv* env,jclass type,jstring root) {
    (void)type;
    if (!env || !root || (*env)->ExceptionCheck(env)) return JNI_FALSE;
    jsize length=(*env)->GetStringUTFLength(env,root);
    if (length<=0 || length>=1000 || (*env)->ExceptionCheck(env)) return JNI_FALSE;
    const char* path=(*env)->GetStringUTFChars(env,root,0);
    if (!path) return JNI_FALSE;
    int result=mobile_text_api_root_valid(path) && mobile_text_deploy_at(path)==0;
    (*env)->ReleaseStringUTFChars(env,root,path);
    return result?JNI_TRUE:JNI_FALSE;
}
