import java.net.*;
import java.util.Enumeration;

public class NetworkUtils {
    public static InetAddress getWiFiAddress() throws SocketException {
        Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();

        while (interfaces.hasMoreElements()) {
            NetworkInterface nif = interfaces.nextElement();

            // Vérifiez si l'interface est active et qu'elle n'est pas une interface loopback
            if (!nif.isUp() || nif.isLoopback() || nif.isVirtual()) {
                continue;
            }

            // Affichez les noms des interfaces détectées (debug)
            //System.out.println("Interface détectée : " + nif.getDisplayName());

            // Vérifiez si l'interface contient "wireless" ou correspond à un nom Wi-Fi connu
            if (nif.getDisplayName().toLowerCase().contains("wi-fi") ||
                nif.getDisplayName().toLowerCase().contains("wireless")) {

                // Parcourez les adresses de l'interface
                Enumeration<InetAddress> addresses = nif.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    InetAddress address = addresses.nextElement();

                    // Recherchez une adresse IPv4
                    if (address instanceof Inet4Address) {
                        System.out.println("Adresse Wi-Fi détectée : " + address.getHostAddress());
                        return address;
                    }
                }
            }
        }

        // Si aucune interface Wi-Fi n'est trouvée
        throw new SocketException("Interface Wi-Fi introuvable ou aucune adresse IPv4 disponible.");
    }

    public static void main(String[] args) {
        try {
            InetAddress wifiAddress = getWiFiAddress();
            System.out.println("Adresse IP de l'interface Wi-Fi : " + wifiAddress.getHostAddress());
        } catch (Exception e) {
            System.err.println("Erreur : " + e.getMessage());
        }
    }
}
