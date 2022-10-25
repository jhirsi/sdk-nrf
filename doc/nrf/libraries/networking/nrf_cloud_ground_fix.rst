.. _lib_nrf_cloud_ground_fix:

nRF Cloud ground fix
##############################

.. contents::
   :local:
   :depth: 2

The nRF Cloud ground fix library enables applications to to request cellular positioning and WiFi positioning data from `nRF Cloud`_.
This library is an enhancement to the :ref:`lib_nrf_cloud` library.

.. note::
   To use the nRF Cloud ground fix positioning service, an nRF Cloud account is needed, and the device needs to be associated with a user's account.

Configuration
*************

Configure the following Kconfig option to enable or disable the use of this library:

* :kconfig:option:`CONFIG_NRF_CLOUD_GROUND_FIX`

Request and process cellular positioning data
*********************************************

The :c:func:`nrf_cloud_ground_fix_request` function is used to request the location of the device.

If requested by the device, nRF Cloud responds with the positioning data and the :c:func:`nrf_cloud_ground_fix_process` function processes the received data.
The function parses the data and returns the location information if it is found.

API documentation
*****************

| Header file: :file:`include/net/nrf_cloud_ground_fix.h`
| Source files: :file:`subsys/net/lib/nrf_cloud/src/`

.. doxygengroup:: nrf_cloud_ground_fix
   :project: nrf
   :members:
