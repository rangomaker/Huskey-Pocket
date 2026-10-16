#pragma once
#include <lvgl.h>
#include "colors.h"
#include "fonts.h"
#include "vibro_manager.h"

#define T9_W   240
#define T9_H   192
#define T9_GAP   3
#define T9_MULTITAP_MS 700

static lv_obj_t*  _t9_root = nullptr;
static lv_obj_t*  _t9_ta   = nullptr;
static void     (*_t9_ok_cb)(void) = nullptr;
static lv_obj_t*  _t9_btns[16] = {};

static int         _t9_pkey   = -1;
static int         _t9_press_ins_n = 0; // Chars insertados por PRESSED actual a revertir en LONG_PRESSED
static int         _t9_pidx   = 0;
static bool        _t9_ins    = false;
static bool        _t9_longfired = false;
static lv_timer_t* _t9_tmr    = nullptr;
static bool        _t9_shift  = false;
static bool        _t9_slock  = false;
static bool        _t9_sym    = false;

// Layout 4x4 (índices 0-15)
// 0=1/.com  1=2/abc  2=3/def  3=⌫
// 4=4/ghi   5=5/jkl  6=6/mno  7=OK
// 8=7/pqrs  9=8/tuv  10=9/wxyz 11=⇧
// 12=SIM    13=0/─   14=.,    15=@

static const char* T9_LETTERS[16] = {
    ".com","abc","def","",
    "ghi","jkl","mno","",
    "pqrs","tuv","wxyz","",
    "","",".,","@"
};
static const char* T9_SYMBOLS[16] = {
    "!?#","@#$","^&*","",
    "()-","_+=","[]{}","",
    "\\|/",":;\"","<>~","",
    "","","`'","€£"
};
static const char T9_NUM[16] = {
    '1','2','3',0,
    '4','5','6',0,
    '7','8','9',0,
    0,'0',0,0
};

static void _t9_refresh_shift_visual() {
    lv_obj_t* btn = _t9_btns[11];
    if(!btn || !lv_obj_is_valid(btn)) return;
    lv_obj_t* sh_ico=lv_obj_get_child(btn,0);
    // Igual que custom_kb: 0=gris, 1=gris+borde azul+ícono azul, 2=azul+ícono blanco
    if(_t9_slock){
        lv_obj_set_style_bg_color(btn,C_ACCENT,0);
        lv_obj_set_style_border_width(btn,0,0);
        if(sh_ico) lv_obj_set_style_text_color(sh_ico,lv_color_hex(0xFFFFFF),0);
    } else if(_t9_shift){
        lv_obj_set_style_bg_color(btn,lv_color_hex(0x3A3A3C),0);
        lv_obj_set_style_border_color(btn,C_ACCENT,0);
        lv_obj_set_style_border_width(btn,2,0);
        if(sh_ico) lv_obj_set_style_text_color(sh_ico,C_ACCENT,0);
    } else {
        lv_obj_set_style_bg_color(btn,lv_color_hex(0x3A3A3C),0);
        lv_obj_set_style_border_width(btn,0,0);
        if(sh_ico) lv_obj_set_style_text_color(sh_ico,lv_color_hex(0xBBBBBB),0);
    }
    // Actualizar labels de teclas de letra según case
    bool up=_t9_shift||_t9_slock;
    for(int ii=0;ii<16;ii++){
        if(!_t9_btns[ii]||!lv_obj_is_valid(_t9_btns[ii])) continue;
        int cc=ii%4;
        if(cc==3||ii==12||ii==13||ii==14||ii==15||ii==0) continue;
        lv_obj_t* lbl_l=lv_obj_get_child(_t9_btns[ii],0);
        if(!lbl_l) continue;
        const char* pool=_t9_sym?T9_SYMBOLS[ii]:T9_LETTERS[ii];
        char buf[8]={};
        for(int c=0;c<(int)strlen(pool)&&c<7;c++)
            buf[c]=up?toupper((unsigned char)pool[c]):pool[c];
        lv_label_set_text(lbl_l,buf);
    }
}

// Restablece mayúscula única tras escribir un carácter
static void _t9_consume_shift_once() {
    if(_t9_shift&&!_t9_slock){
        _t9_shift=false;
        _t9_refresh_shift_visual();
    }
}

static void _t9_commit_cb(lv_timer_t* t) {
    _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
    lv_timer_del(t); _t9_tmr=nullptr;
}
static void _t9_restart_tmr() {
    if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
    _t9_tmr=lv_timer_create(_t9_commit_cb,T9_MULTITAP_MS,nullptr);
    lv_timer_set_repeat_count(_t9_tmr,1);
}

struct _T9Flash { lv_obj_t* b; lv_color_t c; };
static void _t9_flash(lv_obj_t* btn) {
    _T9Flash* ctx = new _T9Flash{btn, lv_obj_get_style_bg_color(btn,0)};
    lv_obj_set_style_bg_color(btn,lv_color_hex(0x2E4A6F),0);
    lv_timer_t* ft=lv_timer_create([](lv_timer_t* t){
        _T9Flash* c=(_T9Flash*)t->user_data;
        if(c->b&&lv_obj_is_valid(c->b)) lv_obj_set_style_bg_color(c->b,c->c,0);
        delete c;
        lv_timer_del(t);
    },150,ctx);
    lv_timer_set_repeat_count(ft,1);
}

static void _t9_ev(lv_event_t* e) {
    lv_event_code_t code=lv_event_get_code(e);
    int ki=(int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t* btn=(lv_obj_t*)lv_event_get_target(e);

    if(ki==7) {
        if(code==LV_EVENT_CLICKED) {
            if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
            _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
            _t9_press_ins_n=0; _t9_longfired=false;
            vibro_pulse();
            if(_t9_ok_cb) _t9_ok_cb();
            else if(_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_event_send(_t9_ta,LV_EVENT_READY,nullptr);
        }
        return;
    }

    if(code==LV_EVENT_LONG_PRESSED_REPEAT) {
        if(ki==3&&_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_textarea_del_char(_t9_ta);
        return;
    }
    if(code==LV_EVENT_LONG_PRESSED) {
        _t9_longfired=true;
        if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
        if(_t9_ins&&_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_textarea_del_char(_t9_ta);
        // Revertir caracteres insertados en PRESSED antes de ingresar número
        while(_t9_press_ins_n>0&&_t9_ta&&lv_obj_is_valid(_t9_ta)){
            lv_textarea_del_char(_t9_ta); _t9_press_ins_n--;
        }
        _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
        if(T9_NUM[ki]&&_t9_ta&&lv_obj_is_valid(_t9_ta)){
            char buf[2]={T9_NUM[ki],0}; lv_textarea_add_text(_t9_ta,buf);
        }
        vibro_pulse(); return;
    }
    if(code!=LV_EVENT_PRESSED) return;
    if(_t9_longfired){ _t9_longfired=false; return; } // suprimir click post-longpress
    _t9_press_ins_n=0; // cada press nuevo arranca sin deuda de inserción

    if(ki!=11) _t9_flash(btn);

    if(ki==3){  // ⌫
        if(_t9_pkey>=0){
            if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
            if(_t9_ins&&_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_textarea_del_char(_t9_ta);
            _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
        } else { if(_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_textarea_del_char(_t9_ta); }
        return;
    }
    if(ki==11){ // ⇧ — 0=minúsculas, 1=próxima letra mayúscula (auto-revert
        // tras esa letra), 2=bloqueo permanente hasta volver a tocar.
        if(!_t9_shift&&!_t9_slock)     {_t9_shift=true;_t9_slock=false;}
        else if(_t9_shift&&!_t9_slock) {_t9_shift=true;_t9_slock=true;}
        else                            {_t9_shift=false;_t9_slock=false;}
        _t9_refresh_shift_visual();
        return;
    }
    if(ki==12){ // SIM
        if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
        _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
        _t9_sym=!_t9_sym;
        lv_obj_set_style_bg_color(btn,_t9_sym?C_ACCENT:lv_color_hex(0x3A3A3C),0);
        // Actualizar labels de teclas de letra
        for(int ii=0;ii<16;ii++){
            if(!_t9_btns[ii]||!lv_obj_is_valid(_t9_btns[ii])) continue;
            int cc=ii%4;
            if(cc==3||ii==12||ii==13||ii==14||ii==15) continue;
            lv_obj_t* lbl_l=lv_obj_get_child(_t9_btns[ii],0);
            if(!lbl_l) continue;
            if(ii==0){
                lv_label_set_text(lbl_l,_t9_sym?T9_SYMBOLS[0]:".com");
            } else {
                const char* pool=_t9_sym?T9_SYMBOLS[ii]:T9_LETTERS[ii];
                char upper[8]={};
                bool up2=_t9_shift||_t9_slock;
                for(int c=0;c<(int)strlen(pool)&&c<7;c++) upper[c]=up2?toupper((unsigned char)pool[c]):pool[c];
                lv_label_set_text(lbl_l,upper);
            }
        }
        return;
    }
    if(ki==13){ // espacio
        if(_t9_pkey>=0){if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;} _t9_ins=false;_t9_pkey=-1;_t9_pidx=0;}
        if(_t9_ta&&lv_obj_is_valid(_t9_ta)){ lv_textarea_add_text(_t9_ta," "); _t9_press_ins_n=1; }
        return;
    }

    // Teclas con letras/símbolos
    const char* pool=_t9_sym?T9_SYMBOLS[ki]:T9_LETTERS[ki];
    if(!pool||!strlen(pool)) return;

    // ki==0 && !_t9_sym → escribir ".com" completo de una vez
    if(ki==0&&!_t9_sym){
        if(_t9_pkey>=0){if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;} _t9_ins=false;_t9_pkey=-1;_t9_pidx=0;}
        if(_t9_ta&&lv_obj_is_valid(_t9_ta)){ lv_textarea_add_text(_t9_ta,".com"); _t9_press_ins_n=4; }
        return;
    }

    if(_t9_pkey==ki){
        if(_t9_ins&&_t9_ta&&lv_obj_is_valid(_t9_ta)) lv_textarea_del_char(_t9_ta);
        _t9_pidx++;
        int len=strlen(pool); char c=pool[_t9_pidx%len];
        if(!_t9_sym&&(_t9_shift||_t9_slock)) c=toupper((unsigned char)c);
        char buf[2]={c,0}; lv_textarea_add_text(_t9_ta,buf); _t9_ins=true;
        _t9_restart_tmr();
        _t9_consume_shift_once();
    } else {
        if(_t9_pkey>=0){
            if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
            _t9_ins=false; _t9_pkey=-1; _t9_pidx=0;
        }
        _t9_pkey=ki; _t9_pidx=0;
        char c=pool[0];
        if(!_t9_sym&&(_t9_shift||_t9_slock)) c=toupper((unsigned char)c);
        char buf[2]={c,0}; lv_textarea_add_text(_t9_ta,buf); _t9_ins=true;
        _t9_restart_tmr();
        _t9_consume_shift_once();
    }
}

static lv_obj_t* custom_kb_t9_create(lv_obj_t* parent, lv_obj_t* ta, void(*ok_cb)(void)) {
    _t9_root=nullptr; _t9_ta=ta; _t9_ok_cb=ok_cb;
    _t9_pkey=-1; _t9_pidx=0; _t9_ins=false; _t9_longfired=false;
    _t9_shift=false; _t9_slock=false; _t9_sym=false;
    if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}

    // Cursor parpadeante en el textarea
    if(ta&&lv_obj_is_valid(ta)){
        lv_obj_set_style_border_color(ta,C_ACCENT,LV_PART_CURSOR|LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(ta,1,LV_PART_CURSOR|LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(ta,LV_OPA_TRANSP,LV_PART_CURSOR|LV_STATE_FOCUSED);
        lv_obj_set_style_border_side(ta,LV_BORDER_SIDE_LEFT,LV_PART_CURSOR|LV_STATE_FOCUSED);
        lv_obj_add_state(ta,LV_STATE_FOCUSED);
    }

    _t9_root=lv_obj_create(parent);
    lv_obj_set_size(_t9_root,T9_W,T9_H);
    lv_obj_set_style_bg_color(_t9_root,lv_color_hex(0x1C1C1E),0);
    lv_obj_set_style_bg_opa(_t9_root,LV_OPA_COVER,0);
    lv_obj_set_style_border_width(_t9_root,0,0);
    lv_obj_set_style_pad_all(_t9_root,T9_GAP,0);
    lv_obj_clear_flag(_t9_root,LV_OBJ_FLAG_SCROLLABLE);

    const int KW=(T9_W-T9_GAP*2-T9_GAP*3)/4;
    const int KH=(T9_H-T9_GAP*2-T9_GAP*3)/4;

    for(int i=0;i<16;i++){
        int col=i%4, row=i/4;
        int x=T9_GAP+col*(KW+T9_GAP);
        int y=T9_GAP+row*(KH+T9_GAP);
        bool isAction=(col==3)||(i==12)||(i==13)||(i==14)||(i==15);

        lv_obj_t* btn=lv_obj_create(_t9_root);
        _t9_btns[i]=btn;
        lv_obj_add_flag(btn,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(btn,KW,KH);
        lv_obj_set_pos(btn,x,y);
        lv_obj_set_style_bg_color(btn,i==7?C_ACCENT:isAction?lv_color_hex(0x3A3A3C):lv_color_hex(0x2C2C2E),0);
        lv_obj_set_style_bg_opa(btn,LV_OPA_COVER,0);
        lv_obj_set_style_border_width(btn,0,0);
        lv_obj_set_style_radius(btn,5,0);
        lv_obj_set_style_pad_all(btn,0,0);

        if(col==3||i==12||i==15){
            lv_obj_t* lbl=lv_label_create(btn);
            if(i==3) lv_label_set_text(lbl,ICO_BACKSPACE);
            else if(i==7) lv_label_set_text(lbl,"OK");
            else if(i==11) lv_label_set_text(lbl,ICO_CAPSLOCK);
            else if(i==12) lv_label_set_text(lbl,"!@#");
            else lv_label_set_text(lbl,T9_LETTERS[i]); // @ o .,
            bool useIcon=(i==3||i==11);
            lv_obj_set_style_text_font(lbl,useIcon?FONT_ICON:FONT_MD,0);
            lv_obj_set_style_text_color(lbl,lv_color_hex(i==7?0xFFFFFF:0xBBBBBB),0);
            lv_obj_center(lbl);
            lv_obj_clear_flag(lbl,LV_OBJ_FLAG_CLICKABLE);
        } else if(i==13){
            lv_obj_t* line=lv_obj_create(btn);
            lv_obj_set_size(line,KW-12,2);
            lv_obj_set_style_bg_color(line,lv_color_hex(0xFFFFFF),0);
            lv_obj_set_style_bg_opa(line,LV_OPA_COVER,0);
            lv_obj_set_style_border_width(line,0,0);
            lv_obj_set_style_pad_all(line,0,0);
            lv_obj_clear_flag(line,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
            lv_obj_center(line);
        } else {
            const char* letras=T9_LETTERS[i];
            char upper[8]={};
            for(int c=0;c<(int)strlen(letras)&&c<7;c++) upper[c]=letras[c];

            lv_obj_t* lbl_l=lv_label_create(btn);
            lv_label_set_text(lbl_l,i==0?".com":upper);
            lv_obj_set_style_text_font(lbl_l,FONT_MD,0);
            lv_obj_set_style_text_color(lbl_l,lv_color_hex(0xFFFFFF),0);
            lv_obj_set_style_text_align(lbl_l,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_set_width(lbl_l,KW);
            lv_obj_set_pos(lbl_l,0,4);
            lv_obj_clear_flag(lbl_l,LV_OBJ_FLAG_CLICKABLE);

            if(T9_NUM[i]){
                char nbuf[2]={T9_NUM[i],0};
                lv_obj_t* lbl_n=lv_label_create(btn);
                lv_label_set_text(lbl_n,nbuf);
                lv_obj_set_style_text_font(lbl_n,FONT_SM,0);
                lv_obj_set_style_text_color(lbl_n,lv_color_hex(0x666666),0);
                lv_obj_set_style_text_align(lbl_n,LV_TEXT_ALIGN_CENTER,0);
                lv_obj_set_width(lbl_n,KW);
                lv_obj_set_pos(lbl_n,0,KH-14);
                lv_obj_clear_flag(lbl_n,LV_OBJ_FLAG_CLICKABLE);
            }
        }

        lv_obj_add_event_cb(btn,_t9_ev,LV_EVENT_PRESSED,(void*)(intptr_t)i);
        if(i==7) lv_obj_add_event_cb(btn,_t9_ev,LV_EVENT_CLICKED,(void*)(intptr_t)i);
        lv_obj_add_event_cb(btn,_t9_ev,LV_EVENT_LONG_PRESSED,(void*)(intptr_t)i);
        lv_obj_add_event_cb(btn,_t9_ev,LV_EVENT_LONG_PRESSED_REPEAT,(void*)(intptr_t)i);
    }
    return _t9_root;
}

static void custom_kb_t9_set_ta(lv_obj_t* ta)   { _t9_ta=ta; }
static lv_obj_t* custom_kb_t9_get_ta()           { return _t9_ta; }
static void custom_kb_t9_hide(){ if(_t9_root) lv_obj_add_flag(_t9_root,LV_OBJ_FLAG_HIDDEN); }
static void custom_kb_t9_show(){ if(_t9_root) lv_obj_clear_flag(_t9_root,LV_OBJ_FLAG_HIDDEN); }
static void custom_kb_t9_reset(){
    if(_t9_tmr){lv_timer_del(_t9_tmr);_t9_tmr=nullptr;}
    _t9_root=nullptr; _t9_ta=nullptr;
    _t9_pkey=-1; _t9_pidx=0; _t9_ins=false; _t9_longfired=false;
    _t9_shift=false; _t9_slock=false; _t9_sym=false;
}
