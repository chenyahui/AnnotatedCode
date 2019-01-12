package tars

import (
	"fmt"
	"net/http"
	"strconv"
	"strings"

	"github.com/TarsCloud/TarsGo/tars/transport"
)

//AddServant add dispatch and interface for object.
/**
*  @ v dispatch 实际上就是application，但是定义更宽泛，可能是client或者server。dispatch的定义在tarsprotocol.go 里面，是个interface。
*  @ f interface{} servant
*  @ obj string
 */
func AddServant(v dispatch, f interface{}, obj string) {
	addServantCommon(v, f, obj, false)
}

//AddServantWithContext add dispatch and interface for object, which have ctx,context
func AddServantWithContext(v dispatch, f interface{}, obj string) {
	addServantCommon(v, f, obj, true)
}

func addServantCommon(v dispatch, f interface{}, obj string, withContext bool) {
	// objRunList的定义在application.go里面，就是一个[]string
	objRunList = append(objRunList, obj)

	// 查找tarsConfig有没有对应的配置，tarsConfig需要在调用AddServant之前，调用tars.GetServerConfig()获取
	cfg, ok := tarsConfig[obj]
	if !ok {
		TLOG.Debug("servant obj name not found ", obj)
		return
	}
	TLOG.Debug("add:", cfg)

	// 新建一个TCPServer
	jp := NewTarsProtocol(v, f, withContext)
	s := transport.NewTarsServer(jp, cfg)

	// 这里是不是就意味着，每个Servant都有独立的ip和port
	goSvrs[obj] = s
}

//AddHttpServant add http servant handler for obj.
func AddHttpServant(mux *TarsHttpMux, obj string) {
	cfg, ok := tarsConfig[obj]
	if !ok {
		TLOG.Debug("servant obj name not found ", obj)
		return
	}
	TLOG.Debug("add http server:", cfg)
	objRunList = append(objRunList, obj)
	appConf := GetServerConfig()
	addrInfo := strings.SplitN(cfg.Address, ":", 2)
	var port int
	if len(addrInfo) == 2 {
		port, _ = strconv.Atoi(addrInfo[1])
	}
	httpConf := &TarsHttpConf{
		Container: appConf.Container,
		AppName:   fmt.Sprintf("%s.%s", appConf.App, appConf.Server),
		Version:   appConf.Version,
		IP:        addrInfo[0],
		Port:      int32(port),
		SetId:     appConf.Setdivision,
	}
	mux.SetConfig(httpConf)
	s := &http.Server{Addr: cfg.Address, Handler: mux}
	httpSvrs[obj] = s
}
