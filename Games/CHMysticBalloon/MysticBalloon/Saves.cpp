#include "Saves.h"
#include <EEPROM.h>

namespace mystic { namespace saves {
Status status=Flash;
uint8_t count=0,active=NO_RUN;
Action action=None;

// Инициализация EEPROM
void begin() {
    EEPROM.begin(512); // Выделяем 512 байт во флэше под симуляцию EEPROM
    Store s;
    if (read(s)) {
        progress=s.prefs;count=s.count;active=s.active;
        status = Flash;
    } else {
        progress=Progress{};
        count=0;active=NO_RUN;
        status = NeedsSetup;
    }
}

uint32_t crc(const void *data,unsigned bytes) {
    auto p=static_cast<const uint8_t *>(data);uint32_t c=0xFFFFFFFFu;
    while (bytes--) { c^=*p++;for (unsigned b=0;b<8;++b) c=(c>>1)^(0xEDB88320u&(0u-(c&1))); }
    return ~c;
}

static bool validPrefs(const Progress &p) {
    return p.level<39 && p.coins<=232 && p.highCoins<=232 && p.options<=15 &&
        p.score<=999999 && p.highScore<=999999;
}

bool valid(const Store &s,unsigned limit) {
    if (!validPrefs(s.prefs) || s.count>limit || s.reserved ||
        (s.count ? s.active>=s.count : s.active!=NO_RUN)) return false;
    for (unsigned i=0;i<s.count;++i) {
        const Run &r=s.runs[i];
        if (r.level>=39 || r.total>232 || r.score>999999 || r.entryScore>r.score || (r.kidFlags&~2047u) ||
            r.vx < -54 || r.vx>54 || r.vy < -96 || r.vy>96 || r.immune>60 ||
            r.jump>11 || (r.balloons>>2)>16 || (r.walkers&0xF000) || r.phase>=40) return false;
        for (unsigned k=0;k<6;++k)
            if (r.hp[k]<-32 || r.hp[k]>30 || r.coinX[k]<-64 || r.coinX[k]>448 ||
                r.walkerX[k]<-64 || r.walkerX[k]>448) return false;
    }
    return true;
}

uint8_t capacity() { return FLASH_RUNS; }

bool read(Store &out) {
    out=Store{};
    
    // Читаем из EEPROM
    uint8_t buf[sizeof(Store)];
    for(size_t i = 0; i < sizeof(Store); i++){
        buf[i] = EEPROM.read(i);
    }
    
    // Проверка сигнатуры и версии
    if (*reinterpret_cast<const uint32_t *>(buf)!=SAVE_MAGIC || buf[4]!=VERSION) return false;
    
    // Проверка CRC
    if (*reinterpret_cast<const uint32_t *>(buf+8+FLASH_BYTES)!=crc(buf,8+FLASH_BYTES)) return false;
    
    out=*reinterpret_cast<const Store *>(buf+8);
    if(valid(out, FLASH_RUNS)) return true;
    
    return false;
}

bool write(Store &s) {
    if (!valid(s,capacity())) return false;
    const uint32_t next=s.generation+1;s.generation=next;
    
    uint8_t buf[512] = {0};
    *reinterpret_cast<uint32_t *>(buf)=SAVE_MAGIC;
    buf[4]=VERSION;buf[5]=0;
    *reinterpret_cast<uint16_t *>(buf+6)=uint16_t(next);
    memcpy(buf+8,&s,sizeof(Store));
    *reinterpret_cast<uint32_t *>(buf+8+FLASH_BYTES)=crc(buf,8+FLASH_BYTES);
    
    // Мягко отключаем только прерывания звукового таймера
    timer1_disable(); 
    
    for(size_t i = 0; i < 8+FLASH_BYTES+4; i++){
        EEPROM.write(i, buf[i]);
    }
    bool ok = EEPROM.commit(); // <-- Самый опасный момент записи во флэш защищен
    
    // Возвращаем таймер к жизни
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); 
    
    if (!ok) { status=WriteFailed; return false; }
    status=Flash;
    count=s.count;active=s.active;return true;
}

bool preferences() {
    Store s;
    read(s);
    const bool changed=s.prefs.highScore!=progress.highScore || s.prefs.highCoins!=progress.highCoins ||
        s.prefs.options!=progress.options;
    if (!changed) return status!=WriteFailed;
    s.prefs.highScore=progress.highScore;s.prefs.highCoins=progress.highCoins;s.prefs.options=progress.options;
    return write(s);
}
bool create() {
    Store s;
    read(s);
    if (s.count>=capacity()) return false;
    s.active=s.count++;s.runs[s.active]=Run{};
    s.prefs.level=s.prefs.coins=0;s.prefs.score=0;
    s.prefs.highScore=progress.highScore;s.prefs.highCoins=progress.highCoins;s.prefs.options=progress.options;
    if (!write(s)) return false;
    progress=s.prefs;return true;
}
bool select(uint8_t index,Run &out) {
    Store s;if (!read(s) || index>=s.count) return false;
    s.active=index;
    const Run &r=s.runs[index];s.prefs.level=r.level;s.prefs.score=r.score;s.prefs.coins=r.total;
    if (!write(s)) return false;
    out=r;progress=s.prefs;return true;
}
bool checkpoint(const Run &run) {
    if (active==NO_RUN) return true; // Diagnostic visits never replace a run.
    if ((run.objects&MID_LEVEL) && !(run.balloons&3)) return preferences();
    Store s;if (!read(s) || active>=s.count) return false;
    const bool same=s.active==active && !memcmp(&s.runs[active],&run,sizeof run) &&
        s.prefs.highScore==progress.highScore && s.prefs.highCoins==progress.highCoins && s.prefs.options==progress.options;
    if (same) return true;
    s.active=active;s.runs[active]=run;
    s.prefs=progress;s.prefs.level=run.level;s.prefs.score=run.score;s.prefs.coins=run.total;
    if (!write(s)) return false;
    progress=s.prefs;return true;
}
uint8_t percent(const Run &r) {
    if (r.kidFlags&512) return 100;
    const unsigned n=r.level==22 ? 4 : 6;
    const unsigned left=__builtin_popcount(unsigned(r.objects&63));
    return ((n-min(n,left))+bool(r.objects&HAS_KEY))*100/(n+1);
}
void label(const Run &r,char *text) {
    memcpy(text,"LEVEL 00 ",9);
    const unsigned level=r.level+1;
    text[6]='0'+level/10;text[7]='0'+level%10;
    fmtInt(text+9,percent(r));
    const unsigned n=strlen(text);text[n]='%';text[n+1]=0;
}
} }