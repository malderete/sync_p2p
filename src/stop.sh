#!/bin/bash
SERVER_PID=`pidof sync_p2p`

if [ -n "$SERVER_PID" ]; then
    echo "Shuting down the IW P2P Server"
    kill $SERVER_PID
else
    echo "No PID found"
fi

