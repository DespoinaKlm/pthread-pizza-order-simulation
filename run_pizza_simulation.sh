#!/bin/bash

CUSTOMER_COUNT=100
INITIAL_SEED=10


echo -e "[\e[1;33;40m+\033[0m] Μεταγλώττιση προγράμματος: pizza_order_simulation.c..."
gcc pizza_order_simulation.c -o pizza_order_simulation -Wall -pthread

# Check if compiled file exists in cwd
if [ -x "pizza_order_simulation" ]; then
    echo -e "[\e[1;32;40m✔\033[0m] Η μεταγλώττιση ολοκληρώθηκε."
else
    echo -e "[\e[1;31;40m✖\033[0m] Η μεταγλώττιση απέτυχε."
    exit 1
fi


# Execution
echo "Εκτέλεση προγράμματος με παραμέτρους: $CUSTOMER_COUNT πελάτες και αρχικό σπόρο $INITIAL_SEED."
echo -e "------------------------------------------------------------------------------------\n"
./pizza_order_simulation $CUSTOMER_COUNT $INITIAL_SEED