app_mongodb
===========

MongoDB Interface for Asterisk PBX
Author: Sokratis Galiatsis <sokratis@techio.com>

BUILDING

1-) Download, build and install mongo-c-driver: http://github.com/mongodb/mongo-c-driver
2-) make

INSTALLING

1-) copy build/app_mongodb.so into Asterisk modules folder (lib/asterisk/modules)

USAGE

1-) Create a config file. See samples/app_mongodb.conf
2-) Setup a new mongoDB db/collection. See samples/sample_schema.json
3-) Add: load => app_mongodb.so in the modules.conf asterisk file

LICENSING

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License v2 as published
by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
