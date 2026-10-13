#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>

// Εργασία 2: στατιστικά διανύσματος με MPI
// μέσος όρος, ελάχιστο, μέγιστο, διασπορά, διάνυσμα Δ (ποσοστό στο εύρος max-min) και prefix sums

int main(int argc, char *argv[]) {
    int rank, size, N, i;
    int *P = NULL;
    int Nprocessor;
    int *Vprocessor;
    double mean;


    MPI_Init(&argc, &argv); // Αρχικοποίηση περιβάλλοντος MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // το rank της καθε διεργασιας αποθηκευεται στην μεταβλητη rank
    MPI_Comm_size(MPI_COMM_WORLD, &size); // ο αριθμος των διεργασιών αποθηκευεται στην μεταβλητη size


// η διεργασια 0 λαμβανει το πληθος των στοιχειων που θα εισαγει ο χρηστης
    if (rank == 0) {
        printf("please type the length of the array: \n");
        fflush(stdout);
        if (scanf("%d", &N) != 1 || N <= 0) {
            printf("Invalid array size. Exiting.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

// δεσμευει χώρο στην μνημη για το διανυσμα και ο χρήστης εισαγει τα στοιχεια του.
        P = (int*) malloc(N * sizeof(int));
        printf("please type the %d elements: \n", N);
        fflush(stdout);
        for (i = 0; i < N; i++) {
            if (scanf("%d", &P[i]) != 1) {
                printf("Invalid number. Exiting.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }
    }

// στέλνεται η μεταβλητη N με τύπο int από την διεργασια 0 σε ολες τις υπόλοιπες διεργασιες
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);


// υπολογιζει το πλήθος που θα επεξεργαστει η καθε διεργασια
// Αν η διαιρεση εχει υπόλοιπο αυτο μπαινει στους πρωτους επεξεργαστες
// (αν N < size, καποιες διεργασιες παιρνουν 0 στοιχεια)
    Nprocessor = N / size + (rank < N % size ? 1 : 0);
    Vprocessor = (int *)malloc((Nprocessor > 0 ? Nprocessor : 1) * sizeof(int));


    int *sendN = NULL;
    int *start = NULL;

//sendN-> Περιέχει πόσα στοιχεία πρέπει να πάρει κάθε διεργασία από το αρχικό διάνυσμα
//start-> Περιέχει τη θέση από την οποία ξεκινάει το τμήμα της κάθε διεργασίας
    if (rank == 0) {
        sendN = (int *)malloc(size * sizeof(int));
        start = (int *)malloc(size * sizeof(int));
        int offset = 0;
        for (int i = 0; i < size; i++) {
            sendN[i] = N / size + (i < N % size ? 1 : 0);
            start[i] = offset;
            offset += sendN[i];
        }
    }

// μοιραζει τα στοιχεια του P στις διεργασιες: η καθε μια παιρνει Nprocessor στοιχεια στον Vprocessor
    MPI_Scatterv(P, sendN, start, MPI_INT, Vprocessor, Nprocessor, MPI_INT, 0, MPI_COMM_WORLD);

// αρχικοποιηση με INT_MAX / INT_MIN και οχι με το Vprocessor[0],
// γιατι μια διεργασια με 0 στοιχεια δεν εχει Vprocessor[0] (θα εστελνε σκουπιδια στο MPI_Reduce)
    int local_min = INT_MAX;
    int local_max = INT_MIN;
    double local_sum = 0.0;

//ευρεση του αθροισματος, του μικροτερου και του μεγαλυτερου για καθε διεργασια ξεχωριστα.
    for (int i = 0; i < Nprocessor; i++) {
        local_sum += Vprocessor[i];
        if (Vprocessor[i] < local_min) local_min = Vprocessor[i];
        if (Vprocessor[i] > local_max) local_max = Vprocessor[i];
    }

    double global_sum = 0.0;
    int global_min;
    int global_max;

//συγκεντρωνει τα local_sum, local_min και local_max απο ολους τους επεξεργαστες στον επεξεργαστη 0
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_min, &global_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_max, &global_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);


// Η διεργασία 0 εκτυπώνει τα αποτελέσματα
    if (rank == 0) {
        mean = global_sum / (double)N;
        printf("Mean: %.2f\n", mean);
        printf("Min: %d\n", global_min);
        printf("Max: %d\n", global_max);
    }

//αποστολή του μεσου ορου στις υπολοιπες διεργασιες για τον υπολογισμο της διασπορας
    MPI_Bcast(&mean, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

//υπολογισμος διασπορας για καθε διεργασια
    double local_diaspora = 0.0;
    for (int i = 0; i < Nprocessor; i++) {
        local_diaspora += ((double)Vprocessor[i] - mean) * ((double)Vprocessor[i] - mean);
    }

    double global_diaspora = 0.0;
    MPI_Reduce(&local_diaspora, &global_diaspora, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Var: %.2f\n", global_diaspora / (double)N);
    }

//αποστολη του ελαχιστου και του μεγιστου σε ολες τις διεργασιες (με MPI_INT, γιατι ειναι int)
    int minmax[2];
    if (rank == 0) {
        minmax[0] = global_min;
        minmax[1] = global_max;
    }
    MPI_Bcast(minmax, 2, MPI_INT, 0, MPI_COMM_WORLD);
    int xmin = minmax[0];
    int XmaxXmin = minmax[1] - minmax[0];

//υπολογισμος του νεου διανυσματος: δi = (xi - xmin) / (xmax - xmin) * 100
//ετσι το μικροτερο στοιχειο γινεται 0% και το μεγαλυτερο 100%.
//Αν ολα τα στοιχεια ειναι ιδια (xmax == xmin) δεν γινεται διαιρεση με το 0, ολα τα δi ειναι 0.
    double *Dprocessor = (double *)malloc((Nprocessor > 0 ? Nprocessor : 1) * sizeof(double));
    for (int i = 0; i < Nprocessor; i++) {
        if (XmaxXmin == 0) {
            Dprocessor[i] = 0.0;
        } else {
            Dprocessor[i] = ((double)(Vprocessor[i] - xmin) / (double)XmaxXmin) * 100.0;
        }
    }

//δημιουργια πινακα για την συλλογη των διανυσματων
    double *D = NULL;
    if (rank == 0) {
        D = (double *)malloc(N * sizeof(double));
    }

//συλλογη των διανυσματων στον πινακα D
    MPI_Gatherv(Dprocessor, Nprocessor, MPI_DOUBLE, D, sendN, start, MPI_DOUBLE, 0, MPI_COMM_WORLD);

//εκτυπωση του νεου διανυσματος απο την διεργασια 0
    if (rank == 0) {
        printf("New vector D:\n");
        for (int i = 0; i < N; i++) {
            printf("%.2f\n", D[i]);
        }

        // Bρισκω το μεγιστο Δ και σε ποιο σημειο(i) εμφανιζεται
        int max_index = 0;
        double max_value = D[0];
        for (int i = 1; i < N; i++) {
            if (D[i] > max_value) {
                max_value = D[i];
                max_index = i;
            }
        }

        //τυπωνω τα αποτελεσματα
        printf("Max D: %.2f\n", max_value);
        printf("At i = %d, with xi = %d\n", max_index + 1, P[max_index]);
    }


//η καθε διεργασια υπολογίζει τα δικά της prefix sums
//(long long για να μην ξεχειλίζει το αθροισμα με μεγαλους αριθμους, malloc αντι για πινακα στη στοιβα)
    long long *local_prefix = (long long *)malloc((Nprocessor > 0 ? Nprocessor : 1) * sizeof(long long));
    long long running = 0;
    for (int i = 0; i < Nprocessor; i++) {
        running += Vprocessor[i];
        local_prefix[i] = running;
    }
    long long local_total = running;   // 0 αν η διεργασια δεν εχει στοιχεια

//το MPI_Scan δινει το αθροισμα ολων των διεργασιων μεχρι και αυτη (inclusive).
//αφαιρωντας το δικο της συνολο μενει το αθροισμα των προηγουμενων διεργασιων (offset).
    long long inclusive = 0;
    MPI_Scan(&local_total, &inclusive, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    long long global_offset = inclusive - local_total;

//προσθετουμε το offset σε ολα τα τοπικα prefix sums ωστε να ξεκινησουν από τη σωστή βάση
    for (int i = 0; i < Nprocessor; i++) {
        local_prefix[i] += global_offset;
    }

//δεσμευση μνημης για τον πινακα global prefix μονο στην διεργασια 0
    long long *global_prefix = NULL;
    if (rank == 0) {
        global_prefix = (long long *)malloc(N * sizeof(long long));
    }

//Συλλογη των local prefix ολων των διεργασιων στον πινακα global prefix.
    MPI_Gatherv(local_prefix, Nprocessor, MPI_LONG_LONG, global_prefix, sendN, start, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

//εκτυπωση των αποτελεσματων
    if (rank == 0) {
        printf("Prefix sums:\n");
        for (int i = 0; i < N; i++) {
            printf("%lld ", global_prefix[i]);
        }
        printf("\n");
        free(global_prefix);
    }

//Αποδεσμευση μνημης
    free(Vprocessor);
    free(Dprocessor);
    free(local_prefix);
//αποδεσμευση μνημης για τους πινακες που η μνημη τους δεσμευτηκε στην διεργασια 0
    if (rank == 0) {
        free(P);
        free(D);
        free(sendN);
        free(start);
    }
    MPI_Finalize();
    return 0;
}
