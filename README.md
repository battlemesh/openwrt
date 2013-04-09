This is an initialized openwrt build environment, for the Wireless Battle of the Mesh.

It's a clone of git://nbd.name/openwrt.git at some point in time; from that point it adds a few commits to include an appropiate feeds.conf and env configs.

The included feeds.conf points the 'packages' feed to git://github.com/battlemesh/openwrt-packages.git , which is a snapshot (at a particular point in time) of the original git://nbd.name/packages.git

So what you get is a particular openwrt trunk revision, of both the base source code and the packages feed.

Compile a firmware with this code
---------------------------------

    git clone -b wbm2013 git://github.com/battlemesh/openwrt.git
    cd openwrt
    set_environment.sh
    make

Update to a newer upstream rev
------------------------------

If you want to bring this snapshot to the current upstream release/revision, you must fetch the upstream changesets, merge them, (and probably push them back to github); do this for both the base and packages repositories.
    
    git clone -b wbm2013 git@github.com:battlemesh/openwrt.git ~/openwrt-wbm
    cd ~/openwrt-wbm
    git pull git://nbd.name/openwrt.git
    
    git clone -b wbm2013 git@github.com:battlemesh/openwrt-packages.git ~/openwrt-wbm-packages
    cd ~/openwrt-wbm-packages
    git pull git://nbd.name/packages.git
