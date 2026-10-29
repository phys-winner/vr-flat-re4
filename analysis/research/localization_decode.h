// Development-only, fixed five localization inputs; never extracts game cache.
#include <errno.h>
typedef struct { const unsigned char* input; unsigned compressed, raw; } LocDecodeBlock;
typedef struct { unsigned locale, raw, count; LocDecodeBlock blocks[128]; } LocDecodeJob;

static int loc_u32(const unsigned char** cursor, const unsigned char* end, unsigned* value) {
    if (end-*cursor < 4) return 0;
    memcpy(value,*cursor,4); *cursor += 4;
    return 1;
}

static int loc_decode_parse(const unsigned char* input, unsigned length, LocDecodeJob jobs[5]) {
    if (length < 12 || memcmp(input,"MLOC\1\0\0\0",8)) return 0;
    const unsigned char* cursor=input+8; const unsigned char* end=input+length;
    unsigned count;
    if (!loc_u32(&cursor,end,&count) || count!=5) return 0;
    for (unsigned index=0;index<5;++index) {
        LocDecodeJob* job=jobs+index;
        if (!loc_u32(&cursor,end,&job->locale) || job->locale!=index ||
            !loc_u32(&cursor,end,&job->raw) || job->raw<16 || job->raw>1048576 ||
            !loc_u32(&cursor,end,&job->count) || !job->count || job->count>128) return 0;
        unsigned remaining=job->raw;
        for (unsigned block=0;block<job->count;++block) {
            LocDecodeBlock* item=job->blocks+block;
            if (!loc_u32(&cursor,end,&item->compressed) || !item->compressed || item->compressed>1048576 ||
                !loc_u32(&cursor,end,&item->raw) || !item->raw || item->raw>remaining ||
                end-cursor<item->compressed) return 0;
            item->input=cursor; cursor+=item->compressed; remaining-=item->raw;
        }
        if (remaining) return 0;
    }
    return cursor==end;
}

#ifndef LOCDECODE_TEST
static int localization_decode(unsigned long base) {
    const unsigned int decoder[8]={0xa9ba6ffc,0xa90167fa,0xa9025ff8,0xa90357f6,0xa9044ff4,0xa9057bfd,0x910143fd,0xd10183ff};
    const unsigned int memory[7]={0xa9bf7bfd,0x910003fd,0xaa1f03e2,0x94000004,0x11004000,0xa8c17bfd,0xd65f03c0};
    const unsigned int compressor[4]={0xd10103ff,0xa9024ff4,0xa9037bfd,0x9100c3fd};
    if (!base || memcmp((void*)(base+0x96b64e0),decoder,sizeof decoder) ||
        memcmp((void*)(base+0x96b4b88),memory,sizeof memory) || memcmp((void*)(base+0x96b6f6c),compressor,sizeof compressor)) {
        LOGF("LOCDECODE refused: native preimage"); return 0;
    }
    int fd=open("/data/data/VRLOC.com.Armature.VR4/mobile_locres_request.bin",O_RDONLY|O_CLOEXEC);
    if (fd<0) { LOGF("LOCDECODE input unavailable"); return 0; }
    off64_t size=lseek64(fd,0,SEEK_END);
    if (size<12 || size>8*1048576 || lseek64(fd,0,SEEK_SET)<0) {close(fd); return 0;}
    unsigned char* input=(unsigned char*)malloc((unsigned)size);
    if (!input) {close(fd);return 0;}
    unsigned at=0;
    while (at<(unsigned)size) {
        ssize_t amount=read(fd,input+at,(unsigned)size-at);
        if (amount<0 && errno==EINTR) continue;
        if (amount<=0) break;
        at+=(unsigned)amount;
    }
    close(fd);
    LocDecodeJob jobs[5]; unsigned char* outputs[5]={0}; int success=0;
    if (at!=(unsigned)size || !loc_decode_parse(input,at,jobs)) goto cleanup;
    int (*chunk)(const void*,long long,void*)=(int(*)(const void*,long long,void*))(base+0x96b6f6c);
    int (*memory_size)(int,long long)=(int(*)(int,long long))(base+0x96b4b88);
    long long (*decode)(const void*,long long,void*,long long,int,int,int,void*,long long,void*,void*,void*,long long,int)=
        (long long(*)(const void*,long long,void*,long long,int,int,int,void*,long long,void*,void*,void*,long long,int))(base+0x96b64e0);
    for (unsigned index=0;index<5;++index) {
        LocDecodeJob* job=jobs+index; outputs[index]=(unsigned char*)malloc(job->raw);
        if (!outputs[index]) goto cleanup;
        unsigned decoded=0;
        for (unsigned block=0;block<job->count;++block) {
            LocDecodeBlock* item=job->blocks+block;
            int compressor_id=chunk(item->input,item->compressed,0);
            if (compressor_id<0 || compressor_id>13) goto cleanup;
            int scratch_bytes=memory_size(compressor_id,item->raw);
            if (scratch_bytes<=0 || scratch_bytes>16*1048576) goto cleanup;
            void* scratch=malloc((unsigned)scratch_bytes);
            if (!scratch) goto cleanup;
            long long result=decode(item->input,item->compressed,outputs[index]+decoded,item->raw,1,1,0,0,0,0,0,scratch,scratch_bytes,3);
            free(scratch);
            if (result!=item->raw) {LOGF("LOCDECODE failure locale=%u block=%u result=%lld",index,block,result);goto cleanup;}
            decoded+=item->raw;
        }
    }
    const char* names[5]={"en","de-DE","es-419","fr-FR","ja-JP"};
    for (unsigned index=0;index<5;++index) {
        char name[128],temporary[132];
        snprintf(name,sizeof name,"/data/data/VRLOC.com.Armature.VR4/mobile_locres_%s.bin",names[index]);
        snprintf(temporary,sizeof temporary,"%s.tmp",name);
        fd=open(temporary,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
        if (fd<0) goto cleanup;
        at=0;
        while (at<jobs[index].raw) {
            ssize_t amount=write(fd,outputs[index]+at,jobs[index].raw-at);
            if (amount<0 && errno==EINTR) continue;
            if (amount<=0) break;
            at+=(unsigned)amount;
        }
        int ok=at==jobs[index].raw && fsync(fd)==0;
        if (close(fd)) ok=0;
        if (!ok || rename(temporary,name)) {unlink(temporary);goto cleanup;}
        LOGF("LOCDECODE accepted locale=%s bytes=%u blocks=%u",names[index],jobs[index].raw,jobs[index].count);
    }
    success=1;
cleanup:
    for (unsigned index=0;index<5;++index) free(outputs[index]);
    free(input);
    LOGF("LOCDECODE finished success=%d",success);
    return success;
}
#endif
