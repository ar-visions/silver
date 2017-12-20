#ifndef _WINDOW_
#define _WINDOW_

#define _Window(D,T,C) _AppDelegate(spr,T,C)         \
    override(D,T,C,void,class_init,(Class))          \
    override(D,T,C,void,init,(C))                    \
    override(D,T,C,void,free,(C))                    \
    override(D,T,C,void,loop,(C))                    \
    method(D,T,C,void,show,(C))                      \
    method(D,T,C,void,destroy,(C))                   \
    method(D,T,C,void,render,(C))                    \
    var(D,T,C,String,title)                          \
    var(D,T,C,bool,resizable)                        \
    var(D,T,C,bool,vsync)                            \
    var(D,T,C,bool,double_buffer)                    \
    var(D,T,C,int,width)                             \
    var(D,T,C,int,height)                            \
    var(D,T,C,int,major_version)                     \
    var(D,T,C,int,minor_version)                     \
    var(D,T,C,GLFWwindow *,window)
declare(Window, AppDelegate);

#endif
