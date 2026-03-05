unset PROJECT

if [ "$1" = "bsp" ]
then
	shift 1
	make -f BspLK.mk $@
	exit
fi