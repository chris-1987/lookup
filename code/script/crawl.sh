#!/bin/sh

#A script for crawling one-day BGP table

#RIPE NCC, Amsterdam
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc00/2016.11/ -e robots=off

#LINX, London
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc01/2016.11/ -e robots=off

#AMS-IX and NL-IX, Amsterdam
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc03/2016.11/ -e robots=off

#CIXP, Geneva
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc04/2016.11/ -e robots=off

#VIX, Vienna
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc05/2016.11/ -e robots=off

#Otemachi, Japan
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc06/2016.11/ -e robots=off

#Stockholm, Sweden
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc07/2016.11/ -e robots=off

#Milan, Italy
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc10/2016.11/ -e robots=off

#New York, USA
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc11/2016.11/ -e robots=off

#Frankfurt, Gemany
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc12/2016.11/ -e robots=off

#Moscow, Russia
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc13/2016.11/ -e robots=off

#Palo Alto, USA
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc14/2016.11/ -e robots=off

#Sao Paulo, Brazil
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc15/2016.11/ -e robots=off

#Miami, USA
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc16/2016.11/ -e robots=off

#Barcelona, Spain
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc18/2016.11/ -e robots=off

#Johannesburg, South Africa
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc19/2016.11/ -e robots=off

#Zurich, Switzerland
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc20/2016.11/ -e robots=off

#paris, France
wget -r -np -nv -A 'bview.20161101.0000.gz, updates.20161101.*.gz' http://data.ris.ripe.net/rrc21/2016.11/ -e robots=off
