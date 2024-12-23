package test_folder;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.util.Scanner;

public class MultiCastSender {
    public static void main(String[] args) {

        System.out.println("Entrez le message a envoyer !! ");
        Scanner sc = new Scanner(System.in);
        String message = sc.nextLine();
        sc.close();
        //String message = "Hello Multicast Group!";
        String multicastAddress = "230.0.0.0"; // Adresse multicast
        int port = 4446; // Port utilisé pour le groupe

        try {
            // Adresse du groupe
            InetAddress group = InetAddress.getByName(multicastAddress);

            // Création du socket multicast
            MulticastSocket socket = new MulticastSocket();

            // Création du message en paquet
            byte[] buffer = message.getBytes();
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length, group, port);

            // Envoi du message au groupe
            socket.send(packet);
            System.out.println("Message envoyé au groupe multicast : " + message);

            // Fermeture du socket
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
