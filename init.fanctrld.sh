#!/bin/sh
#
# ddclient      This shell script takes care of starting and stopping fanctrld.
#
# chkconfig:    345 65 35
# description:  fanctrld controls the fan on an IBM thinkpad
# processname:  ddclient
# config:       /etc/sysconfig/fanctrld

# Source function library.
. /etc/rc.d/init.d/functions

# Source networking configuration.
. /etc/sysconfig/fanctrld

exec=@FANCTRLD@
prog=$(basename $exec)
lockfile=/var/lock/subsys/$prog

start() {
    /sbin/modprobe ibm_acpi
    echo -n $"Starting $prog: "
    daemon $exec $FANCTRLD_OPTIONS
    retval=$?
    echo
    [ $retval -eq 0 ] && touch $lockfile
    return $retval
}

stop() {
    echo -n $"Stopping $prog: "
    killproc $prog
    retval=$?
    echo
    [ $retval -eq 0 ] && rm -f $lockfile
    return $retval
}

restart() {
    stop
    start
}

reload() {
    restart
}

force_reload() {
    restart
}

fdrstatus() {
    status $prog
}

# See how we were called.
case "$1" in
    start|stop|restart|reload)
        $1
        ;;
    force-reload)
        force_reload
        ;;
    status)
        fdrstatus
        ;;
    condrestart)
        [ ! -f $lockfile ] || restart
        ;;
    *)
        echo $"Usage: $0 {start|stop|status|restart|condrestart|reload|force-reload}"
        exit 2
esac
