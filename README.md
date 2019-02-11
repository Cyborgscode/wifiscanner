# wifiscanner
a linux desktop wifiscanner app

how to build:

1) make sure ...

you have iwlist in /usr/sbin/  ( iwlist is i.e. part of Fedora's wireless-tools-29-20 and should be available on any other distro as well )

and you have polkit on your system ( Fedora has and it's usefull in this case )

2) be root .. 

if you wanne build it from scratch :

./make

if your done, or if you wanne take this F28 x64 build of mine, copy files to : 

cp *desktop /usr/share/applications/

cp *policy /usr/share/polkit-1/actions/

cp *ui /usr/share/

cp wifiscanner wifiscanner-gtk /usr/bin/

done. 

Some things are easy ;)
