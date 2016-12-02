#!/bin/bash

#A script for crawling one-day BGP table

bgplist=("rrc00" "rrc01" "rrc03" "rrc04" "rrc05" "rrc06" "rrc07" "rrc10" "rrc11" "rrc12" "rrc13" "rrc14" "rrc15" "rrc16" "rrc18" "rrc19" "rrc20" "rrc21")

#bgplist=("rrc00")

datelist=("01" "02" "03" "04" "05" "06" "07" "08" "09" "10" "11" "12"
	  "13" "14" "15" "16" "17" "18" "19" "20" "21" "22" "23" "24"
	  "25" "26" "27" "28" "29" "30")

for bgp in ${bgplist[@]}; do

	html="http://data.ris.ripe.net/""$bgp""/2016.11/"

	bview_content="bview.20161101.0000.gz"

	wget -r -np -nv -A $bview_content $html -e robots=off

	for date in ${datelist[@]}; do
	
		update_content="updates.201611""$date"".*.gz"
	
		wget -r -np -nv -A $update_content $html -e robots=off
	done
done




