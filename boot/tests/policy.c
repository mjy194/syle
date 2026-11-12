#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../include/syle/boot_platform.h"
struct fake {
    const char *text;
    int read_status, key, launch_status;
    unsigned elapsed, launched, polls, key_after, reads;
    size_t reported_size;
    char log[2048];
    struct syle_boot_config selected;
};
static void log_line(void *ctx, const char *s) {
    struct fake *f = ctx;
    assert(strlen(f->log) + strlen(s) < sizeof f->log);
    strcat(f->log, s);
}
static int read_config(void *ctx, char *p, size_t cap, size_t *size) {
    struct fake *f = ctx;
    ++f->reads;
    size_t n = f->text ? strlen(f->text) : 0;
    assert(n <= cap);
    if (n) memcpy(p, f->text, n);
    *size = f->reported_size ? f->reported_size : n;
    return f->read_status;
}
static int poll_key(void *ctx) { struct fake *f = ctx; ++f->polls; return f->elapsed >= f->key_after ? f->key : 0; }
static void stall(void *ctx, unsigned ms) { ((struct fake *)ctx)->elapsed += ms; }
static int launch(void *ctx, const struct syle_boot_config *c) {
    struct fake *f = ctx;
    f->selected = *c;
    ++f->launched;
    return f->launch_status;
}
static void run(struct fake *f, enum syle_boot_mode selected, unsigned elapsed) {
    struct syle_boot_platform p = { f, log_line, read_config, poll_key, stall, launch };
    int status = syle_boot_run(&p);
    assert(status == (f->launch_status < 0 ? f->launch_status : -1));
    assert(f->launched == 1 && f->selected.mode == selected && f->elapsed == elapsed);
    assert(f->reads == 1 && strstr(f->log,"BOOT.LAUNCH_FAILED"));
}
static void reject(const char *s) {
    struct syle_boot_config c;
    syle_boot_defaults(&c);
    c.mode = SYLE_BOOT_DIAGNOSTIC;
    c.timeout_seconds = 17;
    strcpy(c.title,"unchanged on failure");
    struct syle_boot_config before = c;
    assert(syle_boot_parse(s, strlen(s), &c) == -1);
    assert(!memcmp(&c,&before,sizeof c));
}
int main(void) {
    struct syle_boot_config c;
    assert(!syle_boot_parse(NULL, 0, &c));
    assert(c.mode == SYLE_BOOT_NORMAL && c.timeout_seconds == 3 && !strcmp(c.title,"Syle"));
    struct syle_boot_config before = c;
    assert(syle_boot_parse(NULL,1,&c)==-1 && !memcmp(&c,&before,sizeof c));
    assert(syle_boot_parse("",0,NULL)==-1);
    const char *whitespace="\n\r\n \t\n\tdefault\t=\tnormal\t\n";
    assert(!syle_boot_parse(whitespace,strlen(whitespace),&c));
    assert(c.mode==SYLE_BOOT_NORMAL && c.timeout_seconds==3 && !strcmp(c.title,"Syle"));
    const char *good = " # comment\r\n default = diagnostic \r\n timeout=30\n title = Syle test \n";
    assert(!syle_boot_parse(good, strlen(good), &c));
    assert(c.mode == SYLE_BOOT_DIAGNOSTIC && c.timeout_seconds == 30 && !strcmp(c.title,"Syle test"));
    reject("default=windows"); reject("default=normal\ndefault=normal");
    reject("timeout=31"); reject("timeout=-1"); reject("timeout=1x");
    reject("timeout="); reject("timeout=999999999999999999999999");
    reject("unknown=x"); reject("title="); reject("no equals");
    reject("=value"); reject("timeout=0\ntimeout=1"); reject("title=A\ntitle=B");
    reject("default=normal\ntimeout=0\ntitle=new title\nunknown=x");
    reject("title=abc\txyz"); reject("title=\033[31m");
    reject("title=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    const char nul[] = "title=x\0y";
    assert(syle_boot_parse(nul, sizeof nul-1, &c) == -1);
    assert(syle_boot_parse("", SYLE_BOOT_CONFIG_LIMIT+1, &c) == -1);
    struct fake f = {.read_status=1}; run(&f,SYLE_BOOT_NORMAL,3000);
    assert(strstr(f.log,"CONFIG_MISSING"));
    assert(strstr(f.log,"BOOT MENU"));
    assert(strstr(f.log,"[1]  Start Syle management"));
    assert(strstr(f.log,"[2]  Diagnostic halt"));
    assert(strstr(f.log,"============================================================\n\n"));
    f=(struct fake){.text="default=diagnostic\ntimeout=1"};run(&f,SYLE_BOOT_DIAGNOSTIC,1000);
    f=(struct fake){.text="timeout=0"};run(&f,SYLE_BOOT_NORMAL,0);
    f=(struct fake){.text="timeout=30",.key='2'};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    f=(struct fake){.text="default=diagnostic",.key='1'};run(&f,SYLE_BOOT_NORMAL,0);
    f=(struct fake){.text="default=diagnostic",.key='\r'};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    f=(struct fake){.text="default=diagnostic",.key='\n'};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    f=(struct fake){.text="timeout=1",.key='x'};run(&f,SYLE_BOOT_NORMAL,1000);
    f=(struct fake){.read_status=1,.key=-1};run(&f,SYLE_BOOT_NORMAL,0);
    assert(strstr(f.log,"INPUT_UNAVAILABLE"));
    f=(struct fake){.text="timeout=999"};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    assert(strstr(f.log,"CONFIG_INVALID"));
    assert(f.selected.timeout_seconds==0 && !strcmp(f.selected.title,"Syle") && !f.polls);
    f=(struct fake){.text="default=normal\ntimeout=30\ntitle=discard me\nunknown=x"};
    run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    assert(f.selected.timeout_seconds==0 && !strcmp(f.selected.title,"Syle") && !f.polls);
    f=(struct fake){.read_status=-1};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    f=(struct fake){.reported_size=5000};run(&f,SYLE_BOOT_DIAGNOSTIC,0);
    f=(struct fake){.text="timeout=0",.launch_status=-7};run(&f,SYLE_BOOT_NORMAL,0);
    f=(struct fake){.text="timeout=0",.key='2'};run(&f,SYLE_BOOT_NORMAL,0);
    assert(f.polls==0); /* Zero timeout intentionally does not poll. */
    f=(struct fake){.text="timeout=30"};run(&f,SYLE_BOOT_NORMAL,30000);
    assert(f.polls==300);
    f=(struct fake){.text="timeout=30",.key='2',.key_after=29900};
    run(&f,SYLE_BOOT_DIAGNOSTIC,29900); assert(f.polls==300);
    f=(struct fake){.text="timeout=30",.key='2',.key_after=30000};
    run(&f,SYLE_BOOT_NORMAL,30000); assert(f.polls==300);
    char exact[SYLE_BOOT_CONFIG_LIMIT]; memset(exact, ' ', sizeof exact);
    memcpy(exact,"timeout=30\n#",12);
    assert(!syle_boot_parse(exact,sizeof exact,&c) && c.timeout_seconds==30);
    char title[SYLE_BOOT_TITLE_LIMIT+7];memcpy(title,"title=",6);
    memset(title+6,'x',SYLE_BOOT_TITLE_LIMIT);title[sizeof title-1]=0;
    assert(!syle_boot_parse(title,strlen(title),&c));
    assert(strlen(c.title)==SYLE_BOOT_TITLE_LIMIT);
    reject("default=diag"); reject("title=bad\xff");
    assert(syle_boot_run(NULL)==-1);
    /* Every required callback is validated before the first side effect. */
    for(unsigned missing=0;missing<5;++missing) {
        f=(struct fake){0};
        struct syle_boot_platform platform={&f,log_line,read_config,poll_key,stall,launch};
        switch(missing) {
            case 0: platform.log=NULL; break;
            case 1: platform.read_config=NULL; break;
            case 2: platform.poll_key=NULL; break;
            case 3: platform.stall_ms=NULL; break;
            default: platform.launch=NULL; break;
        }
        assert(syle_boot_run(&platform)==-1);
        assert(!f.reads && !f.polls && !f.elapsed && !f.launched && !f.log[0]);
    }
    puts("boot policy: parser + menu + fallback + callback failure PASS");
}
