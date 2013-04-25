LOCAL_SERVER="192.168.1.1:/www/wbm"
REMOTE_SERVER="fw.qmp.cat:/var/www/firmware/wbm6"
ENVS="tl-wdr mr3040 x86"

if [ "$1" == "compile" ]; then
	for e in $ENVS; do 
		sh set_environment.sh $e
		make package/wbm-testbed/clean
		make -j4
	done
fi

if [ "$1" == "fastcompile" ]; then
	for e in $ENVS; do 
		sh set_environment.sh $e
		make package/wbm-testbed/{clean,install}
		make package/install
		make target/linux/install 
	done
fi


echo "UPLOADING FIRMWARE"

if [ ! -z "$LOCAL_SERVER" ]; then
	scp bin/ar71xx/openwrt-ar71xx-generic-tl-*sysupgrade*.bin $LOCAL_SERVER
	scp bin/x86/openwrt-x86-alix2-combined-ext4.img.gz $LOCAL_SERVER
fi

if [ ! -z "$REMOTE_SERVER" ]; then
	scp bin/ar71xx/openwrt-ar71xx-generic-tl-*sysupgrade*.bin $REMOTE_SERVER
	scp bin/x86/openwrt-x86-alix2-combined-ext4.img.gz $REMOTE_SERVER
fi



