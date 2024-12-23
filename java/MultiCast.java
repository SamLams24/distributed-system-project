package test_folder;

import java.net.InetAddress;
import test_folder.*;

public class MultiCast {
    public static void main(String[] arg) throws Exception { 

        //Lancement du système
        System.out.println("Lancement du Système MultiCast !!");

        if(arg.length==0) {
            System.err.println("Erreur , tableau d'argument vide !!");
            System.exit(1);

        }
         String nom = arg[0];
         InetAddress groupeIP = InetAddress.getByName("239.255.80.84");
         int port = 8084; 
         Emetteur a;
         Recepteur b;

        a = new Emetteur(groupeIP, port, nom);
        b = new Recepteur(groupeIP, port, nom);
    }
 }
