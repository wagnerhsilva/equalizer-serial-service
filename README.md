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
    This takes two arguments, the serial device you want to connect,
    and a debug flag:
    
        ./service /dev/ttyUSB0 <debug_0_or_1>

