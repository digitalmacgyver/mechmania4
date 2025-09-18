set systype = "`uname`"
set myname = "`whoami`"
set mygrp = "`id | sed -e 's/uid=.*(//' | sed -e 's/)//'`"

stty intr ^C

set PATHS=( ~team0/bindirs/$systype /usr/local/bin /local/bin /usr/ucb /bin /usr/bin /usr/X11R6/bin \
	/opt/SUNWspro/bin \
	/usr/ccs/bin /usr/bsd\
	/usr/dcs/software/supported/bin \
	/usr/dcs/software/unsupported/bin \
	/usr/dcs/software/licenced/bin \
	/usr/bin/X11 /usr/openwin/bin /usr/dt/bin ~/bin . )

set path=""
foreach p ( $PATHS )
        if( -d $p ) then
                set path=( $path $p )
        endif
end


stty susp ^Z 
mesg y
 
alias   cls     clear
unalias rm

setenv CVS_RSH ssh
setenv EDITOR vi

if ($?prompt) then
     set    prompt =  "`hostname`_\!> "
endif



unset autologout
