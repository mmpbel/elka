/**
 * @file   elka_drv.c
 * @author Mikhail Palityka (originally Derek Molloy)
 * @date   22 October 2015
 * @brief  A kernel module for controlling a button (or any signal) that is connected to
 * a GPIO. It has full support for interrupts and for sysfs entries so that an interface
 * can be created to the button or the button can be configured from Linux userspace.
 * The sysfs entry appears at /sys/ebb/gpio115
 * @see http://www.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>       // Required for the GPIO functions
#include <linux/interrupt.h>  // Required for the IRQ code
#include <linux/kobject.h>    // Using kobjects for the sysfs bindings
#include <linux/time.h>       // Using the clock to measure time between button presses
#define  DEBOUNCE_TIME 200    ///< The default bounce time -- 200ms

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mikhail Palityka");
MODULE_DESCRIPTION("A simple Linux GPIO Button LKM for the BBB");
MODULE_VERSION("0.1");

static bool isRising = 1;                   ///< Rising edge is the default IRQ property
module_param(isRising, bool, S_IRUGO);      ///< Param desc. S_IRUGO can be read/not changed
MODULE_PARM_DESC(isRising, " Rising edge = 1 (default), Falling edge = 0");  ///< parameter description

#define btnInp_0        30       ///< GPIO_0[30] is 32*0+30=30 PIN_9_11
#define btnInp_1        60       ///< GPIO_1[28] is 32*1+28=60 PIN_9_12
#define btnInp_2        31       ///< GPIO_0[31] is 32*0+31=31 PIN_9_13
#define btnInp_3        50       ///< GPIO_1[18] is 32*1+18=50 PIN_9_14
#define btnInp_4        50       ///< GPIO_2[2]  is 32*2+2=66 PIN_8_07
#define btnInp_5        50       ///< GPIO_2[3]  is 32*2+3=67 PIN_8_08
#define pressDetectInp  2        ///< GPIO_1[19] is 32*0+2 =2  PIN_9_22

static struct gpio btn_gpios[] =
{
    { btnInp_0, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 0" },
    { btnInp_1, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 1" },
    { btnInp_2, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 2" },
    { btnInp_3, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 3" },
    { btnInp_4, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 4" },
    { btnInp_5, GPIOF_DIR_IN | GPIOF_EXPORT_DIR_FIXED, "Btn 5" },
    { pressDetectInp, GPIOF_DIR_IN, "Detect in" }
};

static char   gpioName[8] = "gpioXXX";      ///< Null terminated default string -- just in case
static int    irqNumber;                    ///< Used to share the IRQ number within this file
static int    numberPresses = 0;            ///< For information, store the number of button presses
static int    btnPressedBitMask = 0;        ///< For information, store the bit mask of pressed buttons

static bool   isDebounce = 1;               ///< Use to store the debounce state (on by default)
static struct timespec ts_last, ts_current, ts_diff;  ///< timespecs from linux/time.h (has nano precision)

/// Function prototype for the custom IRQ handler function -- see below for the implementation
static irq_handler_t  ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

/** @brief A callback function to output the numberPresses variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the number of presses
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t numberPresses_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", numberPresses);
}

/** @brief A callback function to read in the numberPresses variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer from which to read the number of presses (e.g., reset to 0).
 *  @param count the number characters in the buffer
 *  @return return should return the total number of characters used from the buffer
 */
static ssize_t numberPresses_store (struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count)
{
    sscanf(buf, "%du", &numberPresses);
    return count;
}

/** @brief A callback function to output the btnPressedBitMask variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the number of presses
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t btnPressedBitMask_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", btnPressedBitMask);
}

/** @brief A callback function to read in the btnPressedBitMask variable
 *  @param kobj represents a kernel object device that appears in the sysfs filesystem
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer from which to read the number of presses (e.g., reset to 0).
 *  @param count the number characters in the buffer
 *  @return return should return the total number of characters used from the buffer
 */
static ssize_t btnPressedBitMask_store (struct kobject *kobj, struct kobj_attribute *attr,
                                   const char *buf, size_t count)
{
    sscanf(buf, "%du", &btnPressedBitMask);
    return count;
}

/** @brief Displays the last time the button was pressed -- manually output the date (no localization) */
static ssize_t lastTime_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%.2lu:%.2lu:%.2lu:%.9lu \n", (ts_last.tv_sec/3600)%24,
          (ts_last.tv_sec/60) % 60, ts_last.tv_sec % 60, ts_last.tv_nsec );
}

/** @brief Display the time difference in the form secs.nanosecs to 9 places */
static ssize_t diffTime_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%lu.%.9lu\n", ts_diff.tv_sec, ts_diff.tv_nsec);
}

/** @brief Displays if button debouncing is on or off */
static ssize_t isDebounce_show (struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
   return sprintf(buf, "%d\n", isDebounce);
}

/** @brief Stores and sets the debounce state */
static ssize_t isDebounce_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
   unsigned int temp;
   sscanf(buf, "%du", &temp);                // use a temp varable for correct int->bool
   gpio_set_debounce(pressDetectInp,0);
   isDebounce = temp;
   if(isDebounce) { gpio_set_debounce(pressDetectInp, DEBOUNCE_TIME);
      printk(KERN_INFO "EBB Button: Debounce on\n");
   }
   else { gpio_set_debounce(pressDetectInp, 0);  // set the debounce time to 0
      printk(KERN_INFO "EBB Button: Debounce off\n");
   }
   return count;
}

/**  Use these helper macros to define the name and access levels of the kobj_attributes
 *  The kobj_attribute has an attribute attr (name and mode), show and store function pointers
 *  The count variable is associated with the numberPresses variable and it is to be exposed
 *  with mode 0666 using the numberPresses_show and numberPresses_store functions above
 */
static struct kobj_attribute count_attr = __ATTR(numberPresses, 0666, numberPresses_show, numberPresses_store);
static struct kobj_attribute debounce_attr = __ATTR(isDebounce, 0666, isDebounce_show, isDebounce_store);
static struct kobj_attribute buttons_attr = __ATTR(btnPressedBitMask, 0666, btnPressedBitMask_show, btnPressedBitMask_store);

/**  The __ATTR_RO macro defines a read-only attribute. There is no need to identify that the
 *  function is called _show, but it must be present. __ATTR_WO can be  used for a write-only
 *  attribute but only in Linux 3.11.x on.
 */
static struct kobj_attribute time_attr  = __ATTR_RO(lastTime);  ///< the last time pressed kobject attr
static struct kobj_attribute diff_attr  = __ATTR_RO(diffTime);  ///< the difference in time attr

/**  The ebb_attrs[] is an array of attributes that is used to create the attribute group below.
 *  The attr property of the kobj_attribute is used to extract the attribute struct
 */
static struct attribute *ebb_attrs[] = {
      &count_attr.attr,                  ///< The number of button presses
      &buttons_attr.attr,                ///< The bit mask of pressed buttons
      &time_attr.attr,                   ///< Time of the last button press in HH:MM:SS:NNNNNNNNN
      &diff_attr.attr,                   ///< The difference in time between the last two presses
      &debounce_attr.attr,               ///< Is the debounce state true or false
      NULL,
};

/**  The attribute group uses the attribute array and a name, which is exposed on sysfs -- in this
 *  case it is gpio115, which is automatically defined in the ebbButton_init() function below
 *  using the custom kernel parameter that can be passed when the module is loaded.
 */
static struct attribute_group attr_group = {
      .name  = gpioName,                 ///< The name is generated in ebbButton_init()
      .attrs = ebb_attrs,                ///< The attributes array defined just above
};

static struct kobject *ebb_kobj;

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point. In this example this
 *  function sets up the GPIOs and the IRQ
 *  @return returns 0 if successful
 */
static int __init ebbButton_init (void)
{
    int result = 0;
    unsigned long IRQflags = IRQF_TRIGGER_RISING;      // The default is a rising-edge interrupt

    printk(KERN_INFO "EBB Button: Initializing the EBB Button LKM\n");
    sprintf(gpioName, "gpio%d", pressDetectInp);           // Create the gpio115 name for /sys/ebb/gpio115

    // create the kobject sysfs entry at /sys/ebb -- probably not an ideal location!
    ebb_kobj = kobject_create_and_add("ebb", kernel_kobj->parent); // kernel_kobj points to /sys/kernel
    if (!ebb_kobj)
    {
        printk(KERN_ALERT "EBB Button: failed to create kobject mapping\n");
        return -ENOMEM;
    }

    // add the attributes to /sys/ebb/ -- for example, /sys/ebb/gpio115/numberPresses
    result = sysfs_create_group(ebb_kobj, &attr_group);
    if (result)
    {
        printk(KERN_ALERT "EBB Button: failed to create sysfs group\n");
        kobject_put(ebb_kobj);                          // clean up -- remove the kobject sysfs entry
        return result;
    }

    getnstimeofday(&ts_last);                          // set the last time to be the current time
    ts_diff = timespec_sub(ts_last, ts_last);          // set the initial time difference to be 0

    // Set button outputs. It is a GPIO in input mode
    result = gpio_request_array(btn_gpios, ARRAY_SIZE(btn_gpios));
    if (result)
    {
        printk(KERN_ALERT "EBB Button: failed to request input array\n");
        return result;
    }

    // Perform a quick test to see that the button is working as expected on LKM load
    printk(KERN_INFO "EBB Button: The button state is currently: %d\n", gpio_get_value(pressDetectInp));

    /// GPIO numbers and IRQ numbers are not the same! This function performs the mapping for us
    irqNumber = gpio_to_irq(pressDetectInp);
    printk(KERN_INFO "EBB Button: The button is mapped to IRQ: %d\n", irqNumber);

    if (!isRising)                           // If the kernel parameter isRising=0 is supplied
    {
        IRQflags = IRQF_TRIGGER_FALLING;      // Set the interrupt to be on the falling edge
    }
    // This next call requests an interrupt line
    result = request_irq(irqNumber,             // The interrupt number requested
                    (irq_handler_t) ebbgpio_irq_handler, // The pointer to the handler function below
                    IRQflags,              // Use the custom kernel param to set interrupt type
                    "ebb_button_handler",  // Used in /proc/interrupts to identify the owner
                    NULL);                 // The *dev_id for shared interrupt lines, NULL is okay
    return result;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit ebbButton_exit(void)
{
    printk(KERN_INFO "EBB Button: The button was pressed %d times\n", numberPresses);
    kobject_put(ebb_kobj);                   // clean up -- remove the kobject sysfs entry
    free_irq(irqNumber, NULL);               // Free the IRQ number, no *dev_id required in this case
    // free inputs
    gpio_free_array(btn_gpios, ARRAY_SIZE(btn_gpios));

    printk(KERN_INFO "EBB Button: Goodbye from the EBB Button LKM!\n");
}

/** @brief The GPIO IRQ Handler function
 *  This function is a custom interrupt handler that is attached to the GPIO above. The same interrupt
 *  handler cannot be invoked concurrently as the interrupt line is masked out until the function is complete.
 *  This function is static as it should not be invoked directly from outside of this file.
 *  @param irq    the IRQ number that is associated with the GPIO -- useful for logging.
 *  @param dev_id the *dev_id that is provided -- can be used to identify which device caused the interrupt
 *  Not used in this example as NULL is passed.
 *  @param regs   h/w specific register values -- only really ever used for debugging.
 *  return returns IRQ_HANDLED if successful -- should return IRQ_NONE otherwise.
 */
static irq_handler_t ebbgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
    int i;
    
    getnstimeofday(&ts_current);         // Get the current time as ts_current
    ts_diff = timespec_sub(ts_current, ts_last);   // Determine the time difference between last 2 presses
    ts_last = ts_current;                // Store the current time as the last time ts_last
    printk(KERN_INFO "EBB Button: The button state is currently: %d\n", gpio_get_value(pressDetectInp));
    numberPresses++;                     // Global counter, will be outputted when the module is unloaded
    
    // read buttons pressed
    btnPressedBitMask = 0;
    for (i=0; i < (ARRAY_SIZE(btn_gpios) - 1); ++i)
    {
        if (gpio_get_value(btn_gpios[i].gpio))
        {
            btnPressedBitMask |= (1 << i);
        }
    }
    
    return (irq_handler_t) IRQ_HANDLED;  // Announce that the IRQ has been handled correctly
}

// This next calls are  mandatory -- they identify the initialization function
// and the cleanup function (as above).
module_init(ebbButton_init);
module_exit(ebbButton_exit);
