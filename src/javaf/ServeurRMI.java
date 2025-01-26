package javaf;

import java.net.*;
import java.rmi.*;
import java.rmi.registry.*;
import java.rmi.server.*;
import java.util.Scanner;

import javaf.MessageTemperature;


public class ServeurRMI extends UnicastRemoteObject implements ServeurRMIInterface {
    private static final String GESTION_CONSOLE_IP = "127.0.0.1"; // Adresse de gestion_console
    private static final int GESTION_CONSOLE_UDP_PORT = 54322;    // Port UDP de gestion_console
    private static final int LOCAL_UDP_PORT = 54325;              // Port local pour recevoir les températures

    private DatagramSocket udpSocket;

    // Constructeur
    protected ServeurRMI() throws RemoteException {
        super();
    }

    // Méthode RMI pour envoyer une commande de chauffage
    @Override
    public void envoyerCommandeChauffage(String piece, int valeur) throws RemoteException {
        try {
            // Construire le message de type MessageTemperature
            MessageTemperature msg = new MessageTemperature(valeur, MessageTemperature.CHAUFFER, piece);

            // Sérialiser le message
            byte[] buffer = msg.toBytes();

            // Envoyer le message via UDP à gestion_console
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length, InetAddress.getByName(GESTION_CONSOLE_IP), GESTION_CONSOLE_UDP_PORT);
            udpSocket.send(packet);

            System.out.println("[ServeurRMI] Commande envoyée : " + msg);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    // Méthode RMI pour recevoir des températures
    @Override
    public MessageTemperature recevoirTemperatures() throws RemoteException {
        try {
            // Recevoir un message UDP
            byte[] buffer = new byte[1024];
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            udpSocket.receive(packet);

            // Désérialiser le message reçu
            MessageTemperature msg = MessageTemperature.fromBytes(buffer, packet.getLength());

            System.out.println("[ServeurRMI] Température reçue : " + msg);
            return msg; // Retourner le message au client RMI
        } catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }

    // Méthode principale du ServeurRMI
    public static void main(String[] args) {
        try {
            // Initialiser le ServeurRMI
            ServeurRMI serveur = new ServeurRMI();

            // Démarrer le registre RMI
            Registry registry = LocateRegistry.createRegistry(1099);
            registry.rebind("ServeurRMI", serveur);
            System.out.println("[ServeurRMI] Serveur RMI en écoute sur le port 1099...");

            // Envoyer le message d'identification au démarrage
            serveur.udpSocket = new DatagramSocket();
            MessageTemperature identificationMsg = new MessageTemperature(0, MessageTemperature.MESURE, "RMI"); // Type IDENTIFICATION
            byte[] buffer = identificationMsg.toBytes();
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length, InetAddress.getByName(GESTION_CONSOLE_IP), GESTION_CONSOLE_UDP_PORT);
            serveur.udpSocket.send(packet);
            System.out.println("[ServeurRMI] Identification envoyée à gestion_console.");

            // Recevoir des messages (températures) via UDP
            serveur.udpSocket = new DatagramSocket(LOCAL_UDP_PORT); // Port local pour recevoir les températures
            System.out.println("[ServeurRMI] Serveur en attente de messages UDP sur le port " + LOCAL_UDP_PORT + "...");

            // Boucle pour continuer à écouter les températures
            while (true) {
                serveur.recevoirTemperatures();
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
