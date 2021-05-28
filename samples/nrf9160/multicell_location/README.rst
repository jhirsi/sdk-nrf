.. _multicell_location:

nRF9160: Multicell location
###########################

.. contents::
   :local:
   :depth: 2

The Multicell location sample demonstrates how to use :ref:`lib_multicell_location` library to get a device's position based on LTE cell measurements.


Requirements
************

The sample supports the following development kits:

.. table-from-rows:: /includes/sample_board_rows.txt
   :header: heading
   :rows: thingy91_nrf9160ns, nrf9160dk_nrf9160ns


.. include:: /includes/spm.txt


Overview
********

You can use this sample as a starting point to implement multicell location services in an application that needs the location of the device.

The sample acquires LTE cell information from :ref:`lte_lc_readme`.
The cell information is passed on to the :ref:`lib_multicell_location` library, where an HTTP request is generated and sent to the location service of choice.
Responses from location services are parsed and returned to the sample, which displays the responses on a terminal.

Currently, the sample can be used with the location service supported by the :ref:`lib_multicell_location` library, which are `HERE Positioning`_ and `Skyhook Precision Location`_.
Before you use the services, see the :ref:`lib_multicell_location` library documentation and the respective location service documentation for required setup.


Trigger location requests
*************************

Location requests can be triggered in the following ways and can be controlled by Kconfig options:

*  Pressing **Button 1**.
*  Periodically, with a configurable interval.
*  Changing of the current LTE cell, indicating that the device has moved.


Configuration
*************

|config|

Setup
=====

To use one of the supported location services, you must have an account and a configured authorization method.
Follow the documentation on `HERE Positioning`_ or `Skyhook Precision Location`_ to create an account and authorization.


Configuration options
=====================

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_REQUEST_PERIODIC - Configuration for periodic request

   This option enables  periodic requests for location.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_REQUEST_PERIODIC_INTERVAL- Configuration for periodic request interval

   This option configures the interval for periodic location requests.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_REQUEST_BUTTON - Configuration for location request button

   This option enables the sample to request location when **Button 1** is pressed.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_REQUEST_CELL_CHANGE -  Configuration for request on cell change

   This option enables the sample to request location when the current LTE cell changes. This can be useful for tests on a moving device.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_PRINT_DATA - Configuration for displaying cell data

   This option enables cell data to be displayed when it is reported from the link controller.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_PSM - Configuration for PSM

   This option enables the sample to request PSM from the network.

.. option:: CONFIG_MULTICELL_LOCATION_SAMPLE_EDRX - Configuration for eDRX

   This option enables the sample to request eDRX from the network.


Library configuration
=====================

Check and configure the following library options that are used by the sample:


* :option:`CONFIG_MULTICELL_LOCATION_SERVICE_HERE` - Enables HERE location service.
* :option:`CONFIG_MULTICELL_LOCATION_SERVICE_SKYHOOK` - Enables Skyhook location service.

For the location service that is used, the authorization method can be set with one of the following options:

* :option:`CONFIG_MULTICELL_LOCATION_HERE_API_KEY` - Configures the HERE location service API key.
* :option:`CONFIG_MULTICELL_LOCATION_SKYHOOK_API_KEY` - Configures the Skyhook location service API key.

See :ref:`lib_multicell_location` for more information on the various configuration options that exist for the services.

Building and running
********************

.. |sample path| replace:: :file:`samples/nrf9160/multicell_location`

.. include:: /includes/build_and_run_nrf9160.txt


Testing
=======

|test_sample|

#. |connect_kit|
#. |connect_terminal|
#. Observe that the sample starts:

   .. code-block:: console

      *** Booting Zephyr OS build v2.4.99-ncs1-4938-g540567aae240  ***
      <inf> multicell_location_sample: Multicell location sample has started
      <inf> multicell_location: Provisioning certificate
      <inf> multicell_location_sample: Connecting to LTE network, this may take several minutes

#. Wait until an LTE connection is established. A successful LTE connection is indicated by the following entry in the log:

   .. code-block:: console

      <inf> multicell_location_sample: Network registration status: Connected - roaming

#. Press **Button 1** on the device to trigger a cell measurement and location request:

   .. code-block:: console

      <inf> multicell_location_sample: Button 1 pressed, starting cell measurements

#. Observe that cell measurements are displayed on the terminal:

   .. code-block:: console

      <inf> multicell_location_sample: Neighbor cell measurements received
      <inf> multicell_location_sample: Current cell:
      <inf> multicell_location_sample:     MCC: 242
      <inf> multicell_location_sample:     MNC: 001
      <inf> multicell_location_sample:     Cell ID: 1654712
      <inf> multicell_location_sample:     TAC: 3410
      <inf> multicell_location_sample:     EARFCN: 1650
      <inf> multicell_location_sample:     Timing advance: 65535
      <inf> multicell_location_sample:     Measurement time: 645008
      <inf> multicell_location_sample:     Physical cell ID: 292
      <inf> multicell_location_sample:     RSRP: 57
      <inf> multicell_location_sample:     RSRQ: 30
      <inf> multicell_location_sample: Neighbor cell 1
      <inf> multicell_location_sample:     EARFCN: 1650
      <inf> multicell_location_sample:     Time difference: -8960
      <inf> multicell_location_sample:     Physical cell ID: 447
      <inf> multicell_location_sample:     RSRP: 33
      <inf> multicell_location_sample:     RSRQ: -17
      <inf>multicell_location_sample: Neighbor cell 2
      <inf> multicell_location_sample:     EARFCN: 100
      <inf> multicell_location_sample:     Time difference: 24
      <inf> multicell_location_sample:     Physical cell ID: 447
      <inf> multicell_location_sample:     RSRP: 19
      <inf> multicell_location_sample:     RSRQ: 4
      <inf> multicell_location_sample: Neighbor cell 3
      <inf> multicell_location_sample:     EARFCN: 3551
      <inf> multicell_location_sample:     Time difference: 32
      <inf> multicell_location_sample:     Physical cell ID: 281
      <inf> multicell_location_sample:     RSRP: 41
      <inf> multicell_location_sample:     RSRQ: 13

#. Confirm that location request is sent, and that the response is received:

   .. code-block:: console

      <inf> multicell_location_sample: Sending location request...
      <inf> multicell_location_sample: Location obtained:
      <inf> multicell_location_sample:     Latitude: 63.4216744
      <inf> multicell_location_sample:     Longitude: 10.4373742
      <inf> multicell_location_sample:     Accuracy: 310

   The request might take a while to complete.

#. Observe that cell measurement and location request happen after the periodic interval has passed:

   .. code-block:: console

      <inf> multicell_location_sample: Periodical start of cell measurements

   Observe that the sample continues with cell measurements and location requests as explained in the previous steps.


Dependencies
************

This sample uses the following |NCS| libraries and drivers:

* :ref:`lib_multicell_location`
* :ref:`lte_lc_readme`
* :ref:`dk_buttons_and_leds_readme`

It uses the following `sdk-nrfxlib`_ library:

* :ref:`nrfxlib:nrf_modem`

It uses the following Zephyr libraries:

* ``include/console.h``
* :ref:`zephyr:kernel_api`:

  * ``include/kernel.h``

In addition, it uses the following samples:

* :ref:`secure_partition_manager`
