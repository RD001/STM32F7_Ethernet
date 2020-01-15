#include "mbed.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/api.h"
#include "eth_arch.h"

//#define USE_DHCP       /* enable DHCP, if disabled static address is used*/
 
/*Static IP ADDRESS*/
#define IP_ADDR0   			192
#define IP_ADDR1   			168
#define IP_ADDR2   			0
#define IP_ADDR3   			10
   
/*NETMASK*/
#define NETMASK_ADDR0   255
#define NETMASK_ADDR1   255
#define NETMASK_ADDR2   255
#define NETMASK_ADDR3   0

/*Gateway Address*/
#define GW_ADDR0   			192
#define GW_ADDR1   			168
#define GW_ADDR2   			0
#define GW_ADDR3   			1 

#define DHCP_OFF                   (uint8_t) 0
#define DHCP_START                 (uint8_t) 1
#define DHCP_WAIT_ADDRESS          (uint8_t) 2
#define DHCP_ADDRESS_ASSIGNED      (uint8_t) 3
#define DHCP_TIMEOUT_APP           (uint8_t) 4
#define DHCP_LINK_DOWN             (uint8_t) 5

#ifdef USE_DHCP
#define MAX_DHCP_TRIES  4
__IO uint8_t DHCP_state = DHCP_OFF;
#endif

struct netif gnetif; /* network interface structure */

DigitalOut led1(LED1);
DigitalOut led3(LED3);

Thread thread1;

static void Netif_Config(void);
void User_notification(struct netif *netif);
#ifdef USE_DHCP
void DHCP_thread(void);
#endif
static void http_server_netconn_thread(void *arg);

int main()
{
    printf("\n\n*** Welcome STM32F767ZI !!! ***\n\r");		
		printf("CLOCK: %d\n\r", OS_Tick_GetClock());	
	
		/* Create tcp_ip stack thread */
		tcpip_init(NULL, NULL);
	
		/* Initialize the LwIP stack */
		Netif_Config();
	
		#ifdef USE_DHCP
			/* Start DHCPClient */	
			thread1.start(DHCP_thread);
		#endif
	
		sys_thread_new("HTTP", http_server_netconn_thread, NULL, DEFAULT_THREAD_STACKSIZE, osPriorityAboveNormal);
	
		/* Notify user about the network interface config */
		User_notification(&gnetif);	

    while (true) {

			
    }
}


/**
  * @brief  Initializes the lwIP stack
  * @param  None
  * @retval None
  */
static void Netif_Config(void)
{
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
 
#ifdef USE_DHCP
  ip_addr_set_zero_ip4(&ipaddr);
  ip_addr_set_zero_ip4(&netmask);
  ip_addr_set_zero_ip4(&gw);
#else
  IP_ADDR4(&ipaddr,IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3);
  IP_ADDR4(&netmask,NETMASK_ADDR0,NETMASK_ADDR1,NETMASK_ADDR2,NETMASK_ADDR3);
  IP_ADDR4(&gw,GW_ADDR0,GW_ADDR1,GW_ADDR2,GW_ADDR3);
#endif /* USE_DHCP */
  
  /* add the network interface */    
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &eth_arch_enetif_init, &tcpip_input);
  
  /*  Registers the default network interface. */
  netif_set_default(&gnetif);
  
  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called.*/
    netif_set_up(&gnetif);
		printf("\n\rEth Configured\n\r");
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
		printf("\n\rLink is down\n\r");
  }
	
}


/**
  * @brief  Notify the User about the network interface config status
  * @param  netif: the network interface
  * @retval None
  */
void User_notification(struct netif *netif) 
{
  if (netif_is_up(netif))
  {
#ifdef USE_DHCP
    /* Update DHCP state machine */
    DHCP_state = DHCP_START;
#else
    /* Turn On LED 1 to indicate ETH and LwIP init success*/
    led1 = 1;
#endif /* USE_DHCP */
  }
  else
  {  
#ifdef USE_DHCP
    /* Update DHCP state machine */
    DHCP_state = DHCP_LINK_DOWN;
#endif  /* USE_DHCP */
   /* Turn On LED 3 to indicate ETH and LwIP init error */
   led3 = 1;
  } 
}


#ifdef USE_DHCP
/**
* @brief  DHCP Process
* @param  argument: network interface
* @retval None
*/
void DHCP_thread(void)
{
  struct netif *netif = &gnetif;
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;
  struct dhcp *dhcp;
  
  for (;;)
  {
    switch (DHCP_state)
    {
			case DHCP_START:
      {
        ip_addr_set_zero_ip4(&netif->ip_addr);
        ip_addr_set_zero_ip4(&netif->netmask);
        ip_addr_set_zero_ip4(&netif->gw);       
        dhcp_start(netif);
        DHCP_state = DHCP_WAIT_ADDRESS;
      }
      break;
      
			case DHCP_WAIT_ADDRESS:
      {                
        if (dhcp_supplied_address(netif)) 
        {
          DHCP_state = DHCP_ADDRESS_ASSIGNED;	
          
          led3 = 0;
					led1 = 1; 
					
					printf("\n\rSTM32 IP: %d.%d.%d.%d\n\r",(((const u8_t*)(&(ipaddr).addr))[0]),(((const u8_t*)(&(ipaddr).addr))[1]),
																								 (((const u8_t*)(&(ipaddr).addr))[2]),(((const u8_t*)(&(ipaddr).addr))[3]));										
        }
        else
        {
          dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);
    
          /* DHCP timeout */
          if (dhcp->tries > MAX_DHCP_TRIES)
          {
            DHCP_state = DHCP_TIMEOUT_APP;
            
            /* Stop DHCP */
            dhcp_stop(netif);
            
            /* Static address used */
            IP_ADDR4(&ipaddr, IP_ADDR0 ,IP_ADDR1 , IP_ADDR2 , IP_ADDR3 );
            IP_ADDR4(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
            IP_ADDR4(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
            netif_set_addr(netif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask), ip_2_ip4(&gw));

            led3 = 0;
						led1 = 1;     
          }
          else
          {
            led3 = 1;
          }
        }
      }
      break;
  
			case DHCP_LINK_DOWN:
			{
				/* Stop DHCP */
				dhcp_stop(netif);
				DHCP_state = DHCP_OFF; 
			}
			break;
			
			default: break;
			
    }
    
    /* wait 250 ms */
    wait_ms(250);
  }
}
#endif  /* USE_DHCP */


/**
  * @brief  Notify the User about the network interface config status
  * @param  netif: the network interface
  * @retval None
  */
static void http_server_netconn_thread(void *arg)
{ 
  struct netconn *conn, *newconn;
  err_t err, accept_err;
  
  /* Create a new TCP connection handle */
  conn = netconn_new(NETCONN_TCP);
  
  if (conn!= NULL)
  {
    /* Bind to port 80 (HTTP) with default IP address */
    err = netconn_bind(conn, NULL, 80);
    
    if (err == ERR_OK)
    {
      /* Put the connection into LISTEN state */
      netconn_listen(conn);
  
      while(1) 
      {
        /* accept any icoming connection */
        accept_err = netconn_accept(conn, &newconn);
        if(accept_err == ERR_OK)
        {
          /* serve connection */
          // Write your code here
					printf("\n\rNew HTTP Connection");

          /* delete connection */
          netconn_delete(newconn);
        }
      }
    }
  }
}
