#include "../include/utils.h"

uint64_t get_mac() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Error opening socket");
        return 1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    // Specify the interface name (e.g., "eth0" or "wlan0")
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
        perror("Error getting MAC address");
        close(sock);
        return 1;
    }

    unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
    // printf("MAC Address: ");
    // for (int i = 0; i < 6; i++) {
    //     printf("%02x", mac[i]);
    //     if (i < 5) printf(":");
    // }
    // printf("\n");

    close(sock);
    return (uint64_t)mac[5];
}
