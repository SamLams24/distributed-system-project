package test_folder;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStreamReader;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;

public class Emetteur extends Thread{
    InetAddress  groupeIP;
    int port;
    MulticastSocket socketEmission;
    String nom;
   
    public Emetteur(InetAddress groupeIP, int port, String nom) throws Exception {
       this.groupeIP = groupeIP;
       this.port = port;
       this.nom = nom;
       socketEmission = new MulticastSocket();
       socketEmission.setTimeToLive(15); // pour un site
       start();
   }
     
   public void run() {
     BufferedReader entreeClavier;
     
     try {
        entreeClavier = new BufferedReader(new InputStreamReader(System.in));
        while(true) {
               String texte = entreeClavier.readLine();
               emettre(texte);
        }
     }
     catch (Exception exc) {
        System.out.println(exc);
     }
   } 
 
   void emettre(String texte) throws Exception {
         byte[] contenuMessage;
         DatagramPacket message;
     
         ByteArrayOutputStream sortie = new ByteArrayOutputStream(); 
         texte = nom + " : " + texte ;
         (new DataOutputStream(sortie)).writeUTF(texte); 
         contenuMessage = sortie.toByteArray();
         message = new DatagramPacket(contenuMessage, contenuMessage.length, groupeIP, port);
         socketEmission.send(message);
   }
 }
