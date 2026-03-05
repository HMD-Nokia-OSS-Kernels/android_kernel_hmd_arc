#!/bin/bash
# Copy .symtab to .fsymtab
# Copy .strtab to .fstrtab
#
# Note: objcopy, GNU objcopy (GNU Binutils) Linaro 2014.11-3-git 2.24.0.20141017,
#       does not support --dump-section and --update-section options, so we use
#       the 'readelf' and 'dd' to finish the copy mission.
#
elffile="$1"

sectionline=`readelf -S $elffile | grep "\.fsymtab" -A1 | xargs | sed "s/\[\s*\S*\s//g"`
fsymtabbase=`echo $sectionline | awk '{print $4}' | sed "s/^/0x0/g"`
fsymtablen=`echo $sectionline| awk '{print $5}' | sed "s/^/0x0/g"`

if [ -z "$sectionline" ];then
  echo "do not support .fsymtab, do nothing"
  exit
fi

sectionline=`readelf -S $elffile | grep "\.fstrtab" -A1 | xargs | sed "s/\[\s*\S*\s//g"`
fstrtabbase=`echo $sectionline | awk '{print $4}' | sed "s/^/0x0/g"`
fstrtablen=`echo $sectionline | awk '{print $5}' | sed "s/^/0x0/g"`
sectionline=`readelf -S $elffile | grep "\.symtab" -A1 | xargs | sed "s/\[\s*\S*\s//g"`
symtabbase=`echo $sectionline | awk '{print $4}' | sed "s/^/0x0/g"`
symtablen=`echo $sectionline | awk '{print $5}' | sed "s/^/0x0/g"`
sectionline=`readelf -S $elffile | grep "\.strtab" -A1 | xargs | sed "s/\[\s*\S*\s//g"`
strtabbase=`echo $sectionline | awk '{print $4}' | sed "s/^/0x0/g"`
strtablen=`echo $sectionline | awk '{print $5}' | sed "s/^/0x0/g"`

cp $elffile $elffile.tmp

len=$(($fsymtablen))
if [ $len -gt $(($symtablen)) ];then
  len=$(($symtablen))
fi
echo "copy .symtab ($symtabbase) to .fsymtab ($fsymtabbase) with len $len"
dd if=$elffile skip=$(($symtabbase)) of=$elffile.tmp seek=$(($fsymtabbase)) bs=1 count=$len conv=notrunc

len=$(($fstrtablen))
if [ $len -gt $(($strtablen)) ];then
  len=$(($strtablen))
fi
echo "copy .strtab ($strtabbase) to .fstrtab ($fstrtabbase) with len $len"
dd if=$elffile skip=$(($strtabbase)) of=$elffile.tmp seek=$(($fstrtabbase)) bs=1 count=$len conv=notrunc

mv $elffile.tmp $elffile
