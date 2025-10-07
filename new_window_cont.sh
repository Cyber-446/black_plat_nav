#!/bin/sh

CONTAINER_NAME="black_plat"  
COMMANDS=". install/setup.bash"  

docker exec -it "$CONTAINER_NAME" /bin/bash -c "$COMMANDS && exec /bin/bash"
