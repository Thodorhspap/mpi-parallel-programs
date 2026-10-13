#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

// Εργασία 1: έλεγχος αν ένας πίνακας είναι ταξινομημένος (αύξουσα σειρά) με MPI_Send / MPI_Recv

int main(int argc, char *argv[]) {
    int rank;
    int size;
    int N;
    int i;
    int *T = NULL;
    int *Tprocessor = NULL;
    int localSorted = 1;
    int globalSorted = 1;
    int firstViolationIndex = -1, globalFirstViolationIndex = -1;

    MPI_Init(&argc, &argv);     // Αρχικοποιηση του περιβαλλοντος MPI.
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // Ληψη του rank της τρεχουσας διεργασιας
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // Ληψη του αριθμου διεργασιων


    // Η διεργασια 0 ζηταει απο τον χρηστη το μεγεθος του πινακα. Αν αυτο ειναι μικροτερο η ισο με το 0 βγαζω μηνυμα λαθους
    if (rank == 0) {
        printf("Enter the size of the array:\n");
        fflush(stdout);
        if (scanf("%d", &N) != 1 || N <= 0) {
            printf("Invalid array size. Exiting.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        // Δεσμευω μνημη για τον πινακα αριθμων και στην συνεχεια ζηταω απο τον χρηστη να συμπληρωσει τον πινακα
        T = (int *)malloc(N * sizeof(int));
        printf("Type the numbers:\n");
        fflush(stdout);
        for (i = 0; i < N; i++) {
            if (scanf("%d", &T[i]) != 1) {
                printf("Invalid number. Exiting.\n");
                MPI_Abort(MPI_COMM_WORLD, 1);
            }
        }

        // στελνω το μεγεθος του πινακα Ν απο την διεργασια 0 στις υπολοιπες διεργασιες με την MPI_Send
        for (int p = 1; p < size; p++) {
            MPI_Send(&N, 1, MPI_INT, p, 0, MPI_COMM_WORLD);
        }

    // οι υπολοιπες διεργασιες λαμβανουν το μεγεθος του πινακα απο την διεργασια 0
    } else {
        MPI_Recv(&N, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    //Υπολογισμος στοιχειων που θα επεξεργαστει η καθε διεργασια
    int processorNumbers = N / size;
    //Υπολογισμος περισσευουμενων στοιχειων που πρεπει να μοιραστουν στις πρωτες διεργασιες
    int remainder = N % size;

    // Υπολογισμος του πρωτου στοιχειου που θα δεχτει η καθε διεργασια
    int firstNumber = rank * processorNumbers + (rank < remainder ? rank : remainder);
    // και του πληθους των στοιχειων της (αν N < size, καποιες διεργασιες παιρνουν 0 στοιχεια)
    int count = processorNumbers + (rank < remainder ? 1 : 0);
    int lastNumber = firstNumber + count;

    // Κατανομη μνημης για τον πινακα αριθμων που εχει η καθε διεργασια
    Tprocessor = (int *)malloc((count > 0 ? count : 1) * sizeof(int));

    if (rank == 0) {
        for (int p = 1; p < size; p++) {
            // sendStart ειναι η θεση του πρωτου στοιχειου που θα σταλει στην διεργασια p
            int sendStart = p * processorNumbers + (p < remainder ? p : remainder);
            int sendCount = processorNumbers + (p < remainder ? 1 : 0);
            MPI_Send(&T[sendStart], sendCount, MPI_INT, p, 0, MPI_COMM_WORLD);

            // η διεργασια 0 τυπωνει τι παιρνει η καθε διεργασια, ετσι η σειρα στην οθονη ειναι παντα σωστη
            printf("Processor %d has numbers: ", p);
            for (i = sendStart; i < sendStart + sendCount; i++) {
                printf("%d ", T[i]);
            }
            printf("\n");
        }

        // Μεταφορα των στοιχειων στην τρεχουσα διεργασια απο τον ολικο πινακα T
        for (i = firstNumber; i < lastNumber; i++) {
            Tprocessor[i - firstNumber] = T[i];
        }
        printf("Processor 0 has numbers: ");
        for (i = 0; i < count; i++) {
            printf("%d ", Tprocessor[i]);
        }
        printf("\n");

    // αν η διεργασια δεν ειναι αυτη με rank 0 λαμβανει απο την διεργασια 0 τα στοιχεια της
    } else {
        MPI_Recv(Tprocessor, count, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    // ελεγχος για την ταξινομηση του τοπικου πινακα της καθε διεργασιας.
    // Η θεση παραβιασης ειναι το στοιχειο που ειναι μικροτερο απο το προηγουμενο του (το i + 1).
    for (i = 0; i < count - 1; i++) {
        if (Tprocessor[i] > Tprocessor[i + 1]) {
            localSorted = 0;
            if (firstViolationIndex == -1) {
                firstViolationIndex = firstNumber + i + 1;
            }
        }
    }

    // Ελεγχος στα ορια των διεργασιων: το πρωτο στοιχειο μιας διεργασιας πρεπει να ειναι >= απο το τελευταιο
    // στοιχειο των προηγουμενων. Στελνεται ζευγαρι {υπαρχει τιμη, τελευταια τιμη}, ωστε μια διεργασια
    // χωρις στοιχεια απλως να το προωθει στην επομενη.
    int prevInfo[2] = {0, 0};
    if (rank > 0) {
        MPI_Recv(prevInfo, 2, MPI_INT, rank - 1, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (count > 0 && prevInfo[0] && prevInfo[1] > Tprocessor[0]) {
            localSorted = 0;
            // το παραβατικο στοιχειο ειναι το πρωτο της διεργασιας, που ειναι πριν απο οποιοδηποτε τοπικο
            firstViolationIndex = firstNumber;
        }
    }

    // αν η τρεχουσα διεργασια δεν ειναι η τελευταια στελνει το τελευταιο στοιχειο της στην επομενη διεργασια
    if (rank < size - 1) {
        int lastInfo[2];
        if (count > 0) {
            lastInfo[0] = 1;
            lastInfo[1] = Tprocessor[count - 1];
        } else {
            lastInfo[0] = prevInfo[0];
            lastInfo[1] = prevInfo[1];
        }
        MPI_Send(lastInfo, 2, MPI_INT, rank + 1, 2, MPI_COMM_WORLD);
    }

    // η διεργασια 0 συγκεντρωνει τα αποτελεσματα απο ολες τις διεργασιες
    if (rank == 0) {
        globalSorted = localSorted;
        globalFirstViolationIndex = firstViolationIndex;

        for (int p = 1; p < size; p++) {
            int recvSorted, recvViolation;

            // ληψη καταστασης ταξινομησης και του δεικτη παραβιασης
            MPI_Recv(&recvSorted, 1, MPI_INT, p, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&recvViolation, 1, MPI_INT, p, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // αν καποια διεργασια δειξει οτι δεν υπαρχει ταξινομηση το globalSorted γινεται 0
            if (!recvSorted) {
                globalSorted = 0;
            }

            // ενημερωση του globalFirstViolationIndex αν η νεα παραβιαση ειναι πιο νωρις
            if (recvViolation != -1 && (globalFirstViolationIndex == -1 || recvViolation < globalFirstViolationIndex)) {
                globalFirstViolationIndex = recvViolation;
            }
        }
    // αποστολη του αποτελεσματος της ταξινομησης και του σημειου λαθους (αν υπαρχει) στην διεργασια 0
    } else {
        MPI_Send(&localSorted, 1, MPI_INT, 0, 3, MPI_COMM_WORLD);
        MPI_Send(&firstViolationIndex, 1, MPI_INT, 0, 4, MPI_COMM_WORLD);
    }

    // εμφανιση αποτελεσματων στην διεργασια 0 (η θεση τυπωνεται ξεκινωντας απο το 1)
    if (rank == 0) {
        if (globalSorted) {
            printf("The array is sorted.\n");
        } else {
            printf("The array is not sorted. First violation at position %d (value %d comes after %d).\n",
                   globalFirstViolationIndex + 1, T[globalFirstViolationIndex], T[globalFirstViolationIndex - 1]);
        }
    }

    // αποδεσμευση μνημης
    free(Tprocessor);
    if (rank == 0) free(T);
    MPI_Finalize();
    return 0;
}
