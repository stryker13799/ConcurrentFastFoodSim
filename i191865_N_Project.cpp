#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <queue>
#include <semaphore.h>
#include <limits>
#include <memory>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
using namespace std;


sem_t sem_waiter;
sem_t sem_cook;
sem_t sem_manager;
sem_t sem_daily_waiter_sales;
unsigned int Daily_Waiter_Sales=0;

class order
{
    public:

    unsigned int customer_ID=0;
    unsigned int price=0;
    unsigned int dishes=0;
    vector<unsigned int>time;

    order(unsigned int id)
    {
        customer_ID=id;
    }
};

class manager
{
    public:
    int Daily_Sales=0;
    queue<order> orders;
    
    int items,cols;

    string menu[4][3]={{"Burger","500","5"},{"Pizza","2000","10"},{"Salad","200","2"},{"Tea","20","1"}};

    manager()
    {
        items=4;
        cols=3;
        Daily_Sales=0;
    }

    void display_menu()
    {
        cout << "\nITEM\t|PRICE\t|TIME\t|NUMBER \n\n";
        for (int i=0;i<items;i++)
        {
            for (int j=0;j<cols;j++)
            {
                cout << menu[i][j] << "\t|";
            }
            cout << i+1 << "\n";
        }
    }
};
manager mgr;

class customer
{   
    public:
    
    unsigned int customer_ID=0;
    string customer_Name="\0";

    customer()
    {
        customer_ID=-1;
        customer_Name="\0";
    }
    customer(unsigned int id,string name)
    {
        this->customer_ID=id;
        this->customer_Name=name;
    }
    void init(vector<unsigned int> &cust_ids)
    {
        cout << "\nEnter ID\n";
        cin>>customer_ID;
        while(std::find(cust_ids.begin(), cust_ids.end(), customer_ID) != cust_ids.end()) //duplicate id found
        {
            cout << "\nDuplicate ID\nPlease enter ID again\n";
            cin>>customer_ID;
        }
        while(cin.fail())
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nError\nEnter ID again\n";
            cin>>customer_ID;
            while(std::find(cust_ids.begin(), cust_ids.end(), customer_ID) != cust_ids.end() || cin.fail()) //duplicate id found
            {   
                cout << "\nDuplicate ID\nPlease enter ID again\n";
                cin>>customer_ID;
            }
        }
        cust_ids.push_back(customer_ID);
        cout << "\nEnter Name\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        getline(cin,this->customer_Name);
    }
};

class cook
{
    public:
    unsigned int wait=0;
    unsigned int dish_number=0;
    unsigned int total_dish=0;
    unsigned int cid=0;

    cook(unsigned int w,unsigned int d,unsigned int t,unsigned int c)
    {
        wait=w;
        dish_number=d;
        total_dish=t;
        cid=c;
    }
};

class initializer
{
    public:

    initializer(unsigned int*waiters, unsigned int*cooks,unsigned int *customers,unsigned int *managers)
    {
        *managers=1;

        cout << "\nEnter number of customers\n";
        cin>>*customers;
        while(cin.fail())
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nError\nEnter number of customers again\n";
            cin>>*customers;
        }
        cout << "\nEnter number of cooks\n";
        cin>>*cooks;
        while(cin.fail())
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nError\nEnter number of cooks again\n";
            cin>>*cooks;
        }
        cout << "\nEnter number of waiters\n";
        cin>>*waiters;
        while(cin.fail())
        {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "\nError\nEnter number of waiters again\n";
            cin>>*waiters;
        }
    }

    void init_semaphores(sem_t *sem_manager,sem_t *sem_cook, sem_t *sem_waiter,unsigned int *NUM_WAITERS,
                        unsigned int *NUM_COOKS,unsigned int *NUM_MANAGERS)
    {
        if(!(!sem_init(sem_waiter,0,*NUM_WAITERS)))
        {
            cout << "\nWaiter Semaphore initialization failed\nExiting...";
            exit(-1);
        }
        if(!(!sem_init(sem_cook,0,*NUM_COOKS)))
        {
            cout << "\nCook Semaphore initializaiton failed\nExiting...";
            exit(-1);
        }
        if(!(!sem_init(sem_manager,0,*NUM_MANAGERS)))
        {
            cout << "\nManager Semaphore initialization failed\nExiting...";
            exit(-1);
        }
        if(!(!sem_init(&sem_daily_waiter_sales,0,1)))
        {
            cout << "\nDaily Waiter Sales Semaphore initialization failed\nExiting...";
            exit(-1);
        }
    }
};


void*cook_thr(void *arg)
{
    sem_wait(&sem_cook);

    cook *ck = static_cast<cook*>(arg);
    int val=0;
    sem_getvalue(&sem_cook,&val);
    cout << "\nCook " << ++val << " preparing order for time: " << ck->wait 
        << ", for customer " << ck->cid << "\n";
    sleep(ck->wait);
    if((!ck->dish_number-ck->total_dish)) //all dishes made
    {
        string s = to_string(ck->cid);
        char const *myfifo = s.c_str();
        int fd=open(myfifo,O_WRONLY);
        write(fd,myfifo,10); //writing to fifo the customer id, signifying that order is completed
        close(fd);
    }
    sem_post(&sem_cook);
    delete ck;
    pthread_exit(NULL);
}

void*waiter_thr(void *arg)
{
    sem_wait(&sem_waiter);

    order *buffer = static_cast<order*>(arg);
    int val=0;
    sem_getvalue(&sem_waiter,&val);
    cout << "\nWaiter " << ++val << " serving food for customer " << buffer->customer_ID << "\n";

    sem_wait(&sem_daily_waiter_sales);
    Daily_Waiter_Sales+=buffer->price;
    sem_post(&sem_daily_waiter_sales);

    delete buffer;

    sem_post(&sem_waiter);
    pthread_exit(NULL);
}

void*customer_thr(void *arg)
{
    sem_wait(&sem_manager);

    customer *cus = static_cast<customer*>(arg);
    cout << "\nI am customer, with name: " << cus->customer_Name << " and ID: " << cus->customer_ID
        << "\nPlease show me menu\n";
    mgr.display_menu();
    unsigned int choice=0;
    cout << "\nEnter corresponding number to select items to order\nEnter 0 to finish placing your order\n";
    order o(cus->customer_ID);
    do
    {
        cin>>choice;
        if(choice>=1&&choice<=4)
        {
            o.price+=stoi(mgr.menu[choice-1][1]);
            o.time.push_back(stoi(mgr.menu[choice-1][2]));
            o.dishes+=1;
        }
        else
        {
            while(!(choice>=1&&choice<=4)&&choice)
            {
                cout << "\nInvalid choice\nPlease enter again\n";
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cin>>choice;
            }
        }
    } while (choice);
    if(!o.dishes)
    {
        cout << "\nCUSTOMER DID NOT PLACE ANY ORDER\n";
        sem_post(&sem_manager);
        delete cus;
        pthread_exit(NULL);
    }

    string s = to_string(cus->customer_ID); //making fifo
    char const *myfifo = s.c_str();
    mkfifo(myfifo,0666);

    mgr.orders.push(o);
    mgr.Daily_Sales+=o.price;
    cout << "\nORDER SUCCESSFULLY PLACED\n";

    sem_post(&sem_manager);
    
    std::unique_ptr<pthread_t[]>tid_cook{new pthread_t[o.dishes]};
    for (int i=0;i<o.dishes;i++)
    {
        cook ck(o.time.at(i),i+1,o.dishes,cus->customer_ID);
        pthread_create(&tid_cook[i],NULL,cook_thr,static_cast<void*>(new cook(ck)));
    }

    int fd=open(myfifo,O_RDONLY); //opening fifo in rdonly mode, to read in future, else threads get blocked

    for (int i=0;i<o.dishes;i++)
    {
        pthread_join(tid_cook[i],NULL);
    }

    char buffer[10];
    read(fd,buffer,sizeof(buffer));
    int tmp=stoi(buffer);
    if(!(o.customer_ID-tmp)) //customer id and id from named pipe are same
    {
        o.customer_ID=tmp;
        waiter_thr(static_cast<void*>(new order(o)));
    }
    else
    {
        cout << "\nERROR\nPlease run program again\nExiting...";
        delete cus;
        exit(-1);
    }
    cout <<"\n\tCUSTOMER " << cus->customer_ID <<", " << cus->customer_Name << " SERVED\n";
    
    delete cus;
    pthread_exit(NULL);
}



int main()
{
    unsigned int NUM_WAITERS,
                 NUM_COOKS,
                 NUM_MANAGERS,
                 NUM_CUSTOMERS;

    vector<unsigned int>cust_ids;
    
    initializer init(&NUM_WAITERS,&NUM_COOKS,&NUM_CUSTOMERS,&NUM_MANAGERS);
    init.init_semaphores(&sem_manager,&sem_cook,&sem_waiter,&NUM_WAITERS,&NUM_COOKS,&NUM_MANAGERS);

    std::unique_ptr<customer[]>c{new customer[NUM_CUSTOMERS]};
    std::unique_ptr<pthread_t[]>tid{new pthread_t[NUM_CUSTOMERS]};

    cout << "\n\tENTER CUSTOMER DETAILS\n";
    for (int i=0;i<NUM_CUSTOMERS;i++)
    {
        cout << "\nEnter details for customer " << i+1 << "\n";
        c[i].init(cust_ids);
    }

    for (int i=0;i<NUM_CUSTOMERS;i++)
    {
        pthread_create(&tid[i],NULL,customer_thr,static_cast<void*>(new customer(c[i])));
    }
    
    for(int i=0;i<NUM_CUSTOMERS;i++)
    {
        pthread_join(tid[i],NULL);
    }
    cout << "\nMANAGER DAILY SALES: " << mgr.Daily_Sales
        << "\nWAITER DAILY SALES: " << Daily_Waiter_Sales<< "\n";

    sem_destroy(&sem_daily_waiter_sales); //destroing semaphores
    sem_destroy(&sem_manager);
    sem_destroy(&sem_waiter);
    sem_destroy(&sem_cook);

    pthread_exit(0);
}