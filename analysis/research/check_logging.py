"""Execute the production logger: one action, identical Android/file payloads."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
LLVM = Path(os.path.join(os.environ['ANDROID_SDK_ROOT'], 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/bin'))
VC = Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
SDK = Path(r'C:\Program Files (x86)\Windows Kits\10')
production = (ROOT / 'analysis/re4fix.c').read_text(encoding='utf-8')
tag = re.search(r'^#define LOG_TAG .+$', production, re.M).group()
start = production.index('#define LOGI(')
logger = production[start:production.index('#include "re4fix_mobile_text.h"', start)]

prefix = r'''
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
typedef ptrdiff_t ssize_t;
#define ANDROID_LOG_INFO 4
#define CLOCK_MONOTONIC 1
#define O_WRONLY 1
#define O_CREAT 2
#define O_TRUNC 4
#define O_APPEND 8
#define O_CLOEXEC 16
static char android_payload[1024], file_payload[1024];
static int android_calls, file_calls, open_calls, close_calls, clock_calls, stub_error;
static int fixture_clock_gettime(int clock, struct timespec* out) {
    if (clock != CLOCK_MONOTONIC) stub_error = 1;
    out->tv_sec = 10; out->tv_nsec = 123000000L + 125000000L * clock_calls++;
    return 0;
}
static int fixture_gettid(void) { return 791; }
static int fixture_open(const char* path, int flags, int mode) {
    if (strcmp(path, "/data/data/com.Armature.VR4/re4fix.log")
        || flags != (O_WRONLY|O_CREAT|O_TRUNC|O_APPEND|O_CLOEXEC) || mode != 0644) stub_error = 1;
    open_calls++; return 23;
}
static int fixture_close(int fd) { (void)fd; close_calls++; return 0; }
static ssize_t fixture_write(int fd, const void* bytes, size_t count) {
    if (fd != 23 || count >= sizeof file_payload) { stub_error = 1; return -1; }
    memcpy(file_payload, bytes, count); file_payload[count] = 0; file_calls++;
    return (ssize_t)count;
}
static int __android_log_vprint(int priority, const char* tag, const char* fmt, va_list ap) {
    if (priority != ANDROID_LOG_INFO || strcmp(tag, "RE4FIX-N")) stub_error = 1;
    android_calls++;
    return vsnprintf(android_payload, sizeof android_payload, fmt, ap);
}
static int __android_log_print(int priority, const char* tag, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int count = __android_log_vprint(priority, tag, fmt, ap);
    va_end(ap); return count;
}
#define clock_gettime fixture_clock_gettime
#define gettid fixture_gettid
#define open fixture_open
#define close fixture_close
#define write fixture_write
'''
suffix = r'''
static int reject_calls, confirm_menu, range_starts;
static int reject_click(void) { reject_calls++; confirm_menu = !confirm_menu; return 1; }
static int range_start(void) { range_starts++; return 1; }
static int same_payload(const char* expected) {
    char expected_file[1024];
    const char* body = strstr(file_payload, "] ");
    snprintf(expected_file, sizeof expected_file, "%s\n", expected);
    return body && !strcmp(android_payload, expected) && !strcmp(body + 2, expected_file);
}
#define CHECK(expression, message) do { if (!(expression)) { fputs(message "\n", stderr); return 1; } } while (0)
int main(void) {
    LOGF("UITAP %s -> %d", "Reject", reject_click());
    CHECK(reject_calls == 1 && confirm_menu == 1, "Reject must execute once and enter confirmation");
    CHECK(android_calls == 1 && file_calls == 1, "Each sink must receive one Reject log");
    CHECK(same_payload("UITAP Reject -> 1"), "Reject payload must match both sinks");
    LOGF("UITAP %s step=%d gain=%.2f serial=%lld -> %d", "RangeStart", 3, 1.25, 4294967297LL, range_start());
    CHECK(range_starts == 1, "Range Start must execute once");
    CHECK(android_calls == 2 && file_calls == 2, "Each sink must receive one Range Start log");
    CHECK(same_payload("UITAP RangeStart step=3 gain=1.25 serial=4294967297 -> 1"), "Mixed varargs must match both sinks");
    CHECK(open_calls == 1 && close_calls == 0 && clock_calls == 2 && !stub_error, "Logger must retain its lazy file/time contract");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='re4-logging-') as temporary:
    work = Path(temporary)
    source, obj, exe = work / 'logging.c', work / 'logging.obj', work / 'logging.exe'

    def run(body):
        source.write_text(prefix + '\n' + tag + '\n' + body + suffix, encoding='utf-8')
        subprocess.run([str(LLVM / 'clang.exe'), '--target=x86_64-pc-windows-msvc', '-O2', '-Werror',
                        '-isystem', str(VC / 'include'), '-isystem', str(SDK / 'Include/10.0.26100.0/ucrt'),
                        '-c', str(source), '-o', str(obj)], check=True)
        subprocess.run([str(VC / 'bin/Hostx64/x64/link.exe'), '/NOLOGO', f'/OUT:{exe}', str(obj),
                        'libcmt.lib', 'libucrt.lib', 'libvcruntime.lib', 'oldnames.lib', 'kernel32.lib',
                        f'/LIBPATH:{VC / "lib/x64"}', f'/LIBPATH:{SDK / "Lib/10.0.26100.0/ucrt/x64"}',
                        f'/LIBPATH:{SDK / "Lib/10.0.26100.0/um/x64"}'], check=True)
        return subprocess.run([str(exe)], text=True, capture_output=True)

    actual = run(logger)
    assert actual.returncode == 0, actual.stderr
    previous, replaced = re.subn(r'^#define LOGF\([^\n]+$',
                                '#define LOGF(...) do { LOGI(__VA_ARGS__); flog(__VA_ARGS__); } while (0)',
                                logger, count=1, flags=re.M)
    assert replaced == 1
    regression = run(previous)
    assert regression.returncode == 1 and 'Reject must execute once' in regression.stderr, regression.stderr

print('PASS: actual C logger evaluates Reject/Range Start once, matches both sink payloads and opens once; previous macro fails the same fixture')
