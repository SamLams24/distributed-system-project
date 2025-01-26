package javaf;

import java.rmi.registry.LocateRegistry;
import java.rmi.registry.Registry;
import java.util.Scanner;
import javaf.MessageTemperature;

public class ClientRMI {
    public static void main(String[] args) {
        try {
            // Recherche du ServeurRMI dans le registre
            Registry registry = LocateRegistry.getRegistry("127.0.0.1", 1099);
            ServeurRMIInterface serveurRMI = (ServeurRMIInterface) registry.lookup("ServeurRMI");

            Scanner scanner = new Scanner(System.in);

            while (true) {
                System.out.println("\n=== Client RMI ===");
                System.out.println("1. Envoyer commande de chauffage");
                System.out.println("2. Recevoir température");
                System.out.println("3. Quitter");
                System.out.print("Choix : ");
                int choix = scanner.nextInt();
                scanner.nextLine(); // Consommer le retour à la ligne

                if (choix == 1) {
                    System.out.print("Nom de la pièce : ");
                    String piece = scanner.nextLine();
                    System.out.print("Valeur de chauffage : ");
                    int valeur = scanner.nextInt();
                    scanner.nextLine();

                    serveurRMI.envoyerCommandeChauffage(piece, valeur);
                } else if (choix == 2) {
                    MessageTemperature temperature = serveurRMI.recevoirTemperatures();
                    System.out.println("Température reçue : " + temperature);
                } else if (choix == 3) {
                    System.out.println("Au revoir !");
                    break;
                } else {
                    System.out.println("Choix invalide.");
                }
            }

            scanner.close();

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

