Added optional :ref:`max_pending_requests
<envoy_v3_api_field_extensions.filters.http.on_demand.v3.OnDemandCds.max_pending_requests>` to limit
HTTP requests waiting for cluster discovery. Excess primary requests receive HTTP 503; excess
mirrored requests are discarded without affecting the primary request.
