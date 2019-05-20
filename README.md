# Serial_AT_tool
test for AT Module

Introduction:

This Project developed for AT Module AutoTest

in CmdList.cfg:   it lists the format of the AT cmd for parsing， it suport the commands as below, user can add new command in the list.it will parse automatically
AT_MYPOWEROFF:    AT$MYPOWEROFF
AT_MYGMR:         AT$MYGMR
AT_MYCCID:        AT$MYCCID
AT_MYNETURC:      AT$MYNETURC
AT_MYNETCON:      AT$MYNETCON
AT_MYNETACT:      AT$MYNETACT
AT_MYIPFILTER:    AT$MYIPFILTER
AT_MYNETSRV:      AT$MYNETSRV
AT_MYNETOPEN:     AT$MYNETOPEN
AT_MYNETREAD:     AT$MYNETREAD
AT_MYNETWRITE:    AT$MYNETWRITE
AT_MYNETCLOSE:    AT$MYNETCLOSE 
AT_MYNETCREATE:   AT$MYNETCREATE
AT_MYFTPOPEN:     AT$MYFTPOPEN
AT_MYFTPCLOSE:    AT$MYFTPCLOSE
AT_MYFTPSIZE:     AT$MYFTPSIZE
AT_MYFTPGET:      AT$MYFTPGET
AT_MYFTPPUT:      AT$MYFTPPUT
AT_MYTYPE:        AT$MYTYPE
AT_MYNETACK:      AT$MYNETACK
AT_MYNETACCEPT:   AT$MYNETACCEPT
AT_MYBCCH:        AT$MYBCCH
AT_MYBAND:        AT$MYBAND
AT_COMMON:


the program was devided to three thread: GUI thread, write Uart thread, read Uart thread,
GUI controls parsing data from the cmd list one by one with a TIMER, each one is send the time will be stopped, wait the reponse data.
Write Uart thread will suspend itself when data is sent
Read uart thread never stop.

any question, welcome to ask me....398866257@qq.com
