    ,'";-------------------;"`.
    ;[]; ................. ;[];
    ;  ; ................. ;  ;
    ;  ; ................. ;  ;
    ;  ; ................. ;  ;
    ;  ; ................. ;  ;
    ;  ; ................. ;  ;
    ;  ; ................. ;  ;
    ;  `.                 ,'  ;
    ;    """""""""""""""""    ;
    ;    ,-------------.---.  ;
    ;    ;  ;"";       ;   ;  ;
    ;    ;  ;  ;       ;   ;  ;
    ;    ;  ;  ;       ;   ;  ;
    ;//||;  ;  ;       ;   ;||;
    ;\\||;  ;__;       ;   ;\/;
     `. _;          _  ;  _;  ;
       " """"""""""" """"" """

Serial service for the cm_comandos project.

Instalation:
    This comes with a Makefile but it is assume your Linux contains
    sqlite3. It also assumes the enviroment which is running has the 
    variable CC set, if that is not the case use [export] to set CC.
    Finnaly simply run:
        make all

Usage:
    This takes a single argument which is the
    serial device you want to connect, a simple invocation is:
    
        ./service /dev/ttyUSB0

Legal:
    This is not a free software if you are not from the following companies 
    you are not allowed to modify/use/redistribute this source code:
        PhiInnovations, CM Comandos Lineares
