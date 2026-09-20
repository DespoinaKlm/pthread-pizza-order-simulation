#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h> // Used for the monitor line ont the bottom that we implemented
#include "pizza_order_simulation.h"


// Define the Order structure, variables and mutexes
typedef struct {
    int id;
    unsigned int seed;
    struct timespec start_time;
} OrderData;

pthread_mutex_t tel_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tel_cond = PTHREAD_COND_INITIALIZER;
int available_tels = Ntel;

pthread_mutex_t cook_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cook_cond = PTHREAD_COND_INITIALIZER;
int available_cooks = Ncook;

pthread_mutex_t oven_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t oven_cond = PTHREAD_COND_INITIALIZER;
int available_ovens = Noven;

pthread_mutex_t deliverer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t deliverer_cond = PTHREAD_COND_INITIALIZER;
int available_deliverers = Ndeliverer;

pthread_mutex_t revenue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

int active_orders = 0;
int total_revenue = 0;
int total_sales_margarita = 0;
int total_sales_pepperoni = 0;
int total_sales_special = 0;
int total_successful_orders = 0;
int total_failed_orders = 0;

long total_service_time = 0;
long max_service_time = 0;
long total_cooling_time = 0;
long max_cooling_time = 0;

struct timespec global_start_time;

// Function to get the terminal height to always print on the bottom
int get_terminal_height() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row;
}

// Function that prints the info for monitoring
void print_info(int cooks, int deliverers, int ovens, int orders) {
    int terminal_height = get_terminal_height();

    // Move cursor to the last line
    printf("\033[%d;0H", terminal_height);
    // Clear the line
    printf("\033[K");
    // Print available cooks and deliverers
    printf("\e[1;37;42mΔιαθέσημοι πόροι: Μάγειρες: %d, Φούρνοι: %d, Διανομείς: %d| Ζωντανές παραγγελίες: %d\033[0m\r", cooks, ovens, deliverers, orders);
    fflush(stdout); // Flush
}

void update(){
    pthread_mutex_lock(&print_mutex);
    print_info(available_cooks, available_deliverers, available_ovens, active_orders);
    pthread_mutex_unlock(&print_mutex);
}


void* handle_order(void* arg) {
    OrderData* order = (OrderData*) arg;
    int oid = order->id;
    unsigned int seed = order->seed;
    struct timespec order_start_time = order->start_time;
    struct timespec end_baking_time, end_pack_time, end_delivery_time;

    // Step 1: Wait for an available answering machine
    pthread_mutex_lock(&tel_mutex);
    while (available_tels == 0) {
        pthread_cond_wait(&tel_cond, &tel_mutex);
    }
    available_tels--;
    pthread_mutex_unlock(&tel_mutex);

    // Order is live
    __sync_fetch_and_add(&active_orders, 1);

    // Step 2: Generate the number of pizzas for the order
    int num_pizzas = (rand_r(&seed) % (Norderhigh - Norderlow + 1)) + Norderlow;

    // Step 3: Simulate payment processing time
    int payment_time = (rand_r(&seed) % (Tpaymenthigh - Tpaymentlow + 1)) + Tpaymentlow;
    sleep(payment_time);

    // Step 4: Check if the payment fails
    if ((rand_r(&seed) % 100) < (Pfail * 100)) {
        pthread_mutex_lock(&print_mutex);
        printf("[\e[1;31;40m✕\033[0m] Η παραγγελία με αριθμό %d απέτυχε.                                              \n", oid);
        print_info(available_cooks, available_deliverers, available_ovens, active_orders);
        pthread_mutex_unlock(&print_mutex);

        // Release the answering machine
        pthread_mutex_lock(&tel_mutex);
        available_tels++;
        pthread_cond_signal(&tel_cond);
        pthread_mutex_unlock(&tel_mutex);

        __sync_fetch_and_add(&total_failed_orders, 1);
        __sync_fetch_and_add(&active_orders, -1);
        free(order);
        return NULL;
    }

    // Step 5: Calculate the order revenue and update sales count
    int order_revenue = 0;
    for (int i = 0; i < num_pizzas; i++) {
        int pizza_type = rand_r(&seed) % 100;
        if (pizza_type < Pm * 100) {
            order_revenue += Cm;
            __sync_fetch_and_add(&total_sales_margarita, 1);
        } else if (pizza_type < (Pm + Pp) * 100) {
            order_revenue += Cp;
            __sync_fetch_and_add(&total_sales_pepperoni, 1);
        } else {
            order_revenue += Cs;
            __sync_fetch_and_add(&total_sales_special, 1);
        }
    }
    __sync_fetch_and_add(&total_revenue, order_revenue);

    pthread_mutex_lock(&print_mutex);
    printf("[\e[1;36;40mi\033[0m] Η παραγγελία με αριθμό %d καταχωρήθηκε.                                          \n", oid);
    print_info(available_cooks, available_deliverers, available_ovens, active_orders);
    pthread_mutex_unlock(&print_mutex);

    // Release the answering machine
    pthread_mutex_lock(&tel_mutex);
    available_tels++;
    pthread_cond_signal(&tel_cond);
    pthread_mutex_unlock(&tel_mutex);

    // Step 6: Wait for an available cook
    pthread_mutex_lock(&cook_mutex);
    while (available_cooks == 0) {
        pthread_cond_wait(&cook_cond, &cook_mutex);
    }
    available_cooks--;
    pthread_mutex_unlock(&cook_mutex);

    update();

    // Step 7: Prepare the pizzas
    sleep(Tprep * num_pizzas);

    // Step 8: Wait for enough available ovens
    pthread_mutex_lock(&oven_mutex);
    while (available_ovens < num_pizzas) {
        pthread_cond_wait(&oven_cond, &oven_mutex);
    }
    available_ovens -= num_pizzas;
    pthread_mutex_unlock(&oven_mutex);

    update();

    // Release the cook right after he put the pizzas in the ovens
    pthread_mutex_lock(&cook_mutex);
    available_cooks++;
    pthread_cond_signal(&cook_cond);
    pthread_mutex_unlock(&cook_mutex);

    // Step 9: Bake the pizzas
    sleep(Tbake);
    clock_gettime(CLOCK_REALTIME, &end_baking_time);

    update();

    // Step 10: Wait for an available deliverer
    pthread_mutex_lock(&deliverer_mutex);
    while (available_deliverers == 0) {
        pthread_cond_wait(&deliverer_cond, &deliverer_mutex);
    }

    // Release the ovens since there is a deliverer available to pack
    pthread_mutex_lock(&oven_mutex);
    available_ovens += num_pizzas;
    pthread_cond_broadcast(&oven_cond);
    pthread_mutex_unlock(&oven_mutex);
    
    update();

    available_deliverers--;
    pthread_mutex_unlock(&deliverer_mutex);

    update();

    // Step 11: Pack the pizzas
    sleep(Tpack * num_pizzas);

    clock_gettime(CLOCK_REALTIME, &end_pack_time);

    pthread_mutex_lock(&print_mutex);
    printf("[\e[1;33;40m+\033[0m] Η παραγγελία με αριθμό %d ετοιμάστηκε σε %ld λεπτά.                              \n", oid, end_pack_time.tv_sec - order_start_time.tv_sec);
    print_info(available_cooks, available_deliverers, available_ovens, active_orders);
    pthread_mutex_unlock(&print_mutex);

    // Step 12: Deliver the pizzas
    int delivery_time = (rand_r(&seed) % (Tdelhigh - Tdellow + 1)) + Tdellow;
    sleep(delivery_time);

    clock_gettime(CLOCK_REALTIME, &end_delivery_time);
   
    // Print finished delivery
    pthread_mutex_lock(&print_mutex);
    printf("[\e[1;32;40m✔\033[0m] Η παραγγελία με αριθμό %d παραδόθηκε σε %ld λεπτά.                               \n", oid, end_delivery_time.tv_sec - order_start_time.tv_sec);
    print_info(available_cooks, available_deliverers, available_ovens, active_orders);
    pthread_mutex_unlock(&print_mutex);

    // Step 13: Return to the store
    sleep(delivery_time);
    // Release the deliverer
    pthread_mutex_lock(&deliverer_mutex);
    available_deliverers++;
    pthread_cond_signal(&deliverer_cond);
    pthread_mutex_unlock(&deliverer_mutex);

    // Order compeleted and deliverer is back 
    __sync_fetch_and_add(&active_orders, -1);

    update();

    // Calculate and update service time and cooling time statistics
    long service_time = end_delivery_time.tv_sec - order_start_time.tv_sec;
    long cooling_time = end_delivery_time.tv_sec - end_baking_time.tv_sec;

    pthread_mutex_lock(&stats_mutex);
    total_service_time += service_time;
    if (service_time > max_service_time) {
        max_service_time = service_time;
    }
    total_cooling_time += cooling_time;
    if (cooling_time > max_cooling_time) {
        max_cooling_time = cooling_time;
    }
    pthread_mutex_unlock(&stats_mutex);


    __sync_fetch_and_add(&total_successful_orders, 1);
    free(order);
    pthread_exit(NULL);

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Ncust> <Seed>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int Ncust = atoi(argv[1]);
    unsigned int Seed = atoi(argv[2]);

    srand(Seed);
    clock_gettime(CLOCK_REALTIME, &global_start_time);

    pthread_t threads[Ncust];

    print_info(available_cooks, available_deliverers, available_ovens, active_orders);
    for (int i = 0; i < Ncust; i++) {
        OrderData* order_data = (OrderData*) malloc(sizeof(OrderData));
        // Check if there is enough memory to create the OrderData for the thread
        if (order_data == NULL) {
            perror("[\e[1;31;40mERROR\033[0m] Failed to allocate memory for order data");
            return EXIT_FAILURE;
        }
        print_info(available_cooks, available_deliverers, available_ovens, active_orders);


        order_data->id = i + 1;
        order_data->seed = Seed + i;
        clock_gettime(CLOCK_REALTIME, &order_data->start_time);

        // Check for unexpected error during the thread creation
        if (pthread_create(&threads[i], NULL, handle_order, order_data) != 0) {
            perror("[\e[1;31;40mERROR\033[0m] Failed to create thread");
            free(order_data);
            return EXIT_FAILURE;
        }

        int next_call_interval = (rand() % (Torderhigh - Torderlow + 1)) + Torderlow;
        sleep(next_call_interval);
    }

    // Wait for all threads to finish their jobs and then add them to this main thread
    for (int i = 0; i < Ncust; i++) {
        pthread_join(threads[i], NULL);
    }

    long avg_service_time = total_successful_orders > 0 ? total_service_time / total_successful_orders : 0;
    long avg_cooling_time = total_successful_orders > 0 ? total_cooling_time / total_successful_orders : 0;

    printf("                                                                                     \n");
    printf("\e[1;37;40m=================================== Στατιστικά =====================================\033[0m");
    printf("\n[\e[1;32;40m$\033[0m] Συνολικά έσοδα: %d euros.\n", total_revenue);
    printf("[\e[1;36;40mi\033[0m] Πίτσες μαργαρίτα από παραγγελίες: %d.\n", total_sales_margarita);
    printf("[\e[1;36;40mi\033[0m] Πίτσες πεπερόνι από παραγγελίες: %d.\n", total_sales_pepperoni);
    printf("[\e[1;36;40mi\033[0m] Πίτσες σπέσιαλ από παραγγελίες: %d.\n", total_sales_special);
    printf("[\e[1;32;40m✔\033[0m] Επιτυχημένες παραγγελίες: %d.\n", total_successful_orders);
    printf("[\e[1;31;40m✕\033[0m] Αποτυχημένες παραγγελίες: %d.\n", total_failed_orders);
    printf("[\e[1;36;40mi\033[0m] Μέσος χρόνος εξυπηρέτησης πελατών: %ld λεπτά.\n", avg_service_time);
    printf("[\e[1;36;40mi\033[0m] Μέγιστος χρόνος εξυπηρέτησης πελατών: %ld λεπτά.\n", max_service_time);
    printf("[\e[1;36;40mi\033[0m] Μέσος χρόνος κρυώματος παραγγελιών: %ld λεπτά.\n", avg_cooling_time);
    printf("[\e[1;36;40mi\033[0m] Μέγιστος χρόνος κρυώματος παραγγελιών: %ld λεπτά.\n", max_cooling_time);

    return EXIT_SUCCESS;
}