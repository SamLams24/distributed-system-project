package test_folder.java;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;

public class Recepteur extends Thread {
    InetAddress  groupeIP;
    int port;
    String nom;
    MulticastSocket socketReception;
 
    public Recepteur(InetAddress groupeIP, int port, String nom)  throws Exception { 
        this.groupeIP = groupeIP;
        this.port = port;
        this.nom = nom;
        socketReception = new MulticastSocket(port);
        socketReception.joinGroup(groupeIP);
        start();
   }
 
   public void run() {
     DatagramPacket message;
     byte[] contenuMessage;
     String texte;
     ByteArrayInputStream lecteur;
     
     while(true) {
           contenuMessage = new byte[1024];
           message = new DatagramPacket(contenuMessage, contenuMessage.length);
           try {
                   socketReception.receive(message);
                   texte = (new DataInputStream(new ByteArrayInputStream(contenuMessage))).readUTF();
            if (!texte.startsWith(nom)) continue;
            System.out.println(texte);
           }
           catch(Exception exc) {
                 System.out.println(exc);
           }
     }
   }
 }
