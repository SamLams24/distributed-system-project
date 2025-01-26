package javaf;

import java.rmi.Remote;
import java.rmi.RemoteException;
import javaf.MessageTemperature;

public interface ServeurRMIInterface extends Remote {
    void envoyerCommandeChauffage(String piece, int valeur) throws RemoteException;

    MessageTemperature recevoirTemperatures() throws RemoteException;
}

