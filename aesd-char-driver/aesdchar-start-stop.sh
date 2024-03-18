#! /bin/sh
case "$1" in
    start)
    echo "Loading Module aesdchar"
    aesdchar_load
    ;;
    stop)
    echo "Unloading module aesdchar"
    aesdchar_unload
    ;;
    *)
    echo "Usage $0 {start|stop}"
    exit 1
    ;;
esac

exit 0