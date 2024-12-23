package test_folder;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;

public class MultiCastReceiver {
    @SuppressWarnings("deprecation")
    public static void main(String[] args) {
        String multicastAddress = "230.0.0.0"; // Adresse multicast (doit correspondre à l'émetteur)
        int port = 4446; // Port utilisé pour le groupe

        try {
            // Adresse du groupe
            InetAddress group = InetAddress.getByName(multicastAddress);

            // Création du socket multicast
            MulticastSocket socket = new MulticastSocket(port);

            // Rejoindre le groupe multicast
            socket.joinGroup(group);
            System.out.println("Rejoint le groupe multicast sur " + multicastAddress + ":" + port);

            // Réception des messages
            byte[] buffer = new byte[256];
            while (true) {
                DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
                socket.receive(packet);

                String received = new String(packet.getData(), 0, packet.getLength());
                System.out.println("Message reçu : " + received);

                // Quitte si un message particulier est reçu
                if (received.equals("exit")) {
                    System.out.println("Fin de la réception.");
                    break;
                }
            }

            // Quitter le groupe et fermer le socket
            socket.leaveGroup(group);
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
