go-restful在启动的时候，通常要用这一句

```
http.ListenAndServe(":8080", nil)
```

第二个参数是handler，但是确是nil，根据源码注释来看 DefaultServeMux