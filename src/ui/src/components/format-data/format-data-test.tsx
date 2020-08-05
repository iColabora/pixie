import { getClasses } from '@material-ui/core/test-utils';
import { mount } from 'enzyme';
import * as React from 'react';
import { AlertData, JSONData, LatencyData } from './format-data';

describe('<LatencyData/> test', () => {
  const classes = getClasses(<LatencyData data='20' />);

  it('should render correctly for low latency', () => {
    const wrapper = mount(<LatencyData data='20' />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.low);
    expect(wrapper.find('div').text()).toEqual('20');
  });

  it('should render correctly for medium latency', () => {
    const wrapper = mount(<LatencyData data='160' />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.med);
    expect(wrapper.find('div').text()).toEqual('160');
  });

  it('should render correctly for high latency', () => {
    const wrapper = mount(<LatencyData data='350' />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.high);
    expect(wrapper.find('div').text()).toEqual('350');
  });
});

describe('<AlertData/> test', () => {
  const classes = getClasses(<AlertData data />);

  it('should render correctly for true alert', () => {
    const wrapper = mount(<AlertData data />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.true);
    expect(wrapper.find('div').text()).toEqual('true');
  });

  it('should render correctly for false alert', () => {
    const wrapper = mount(<AlertData data={false} />);

    expect(wrapper.find('div').prop('className')).toEqual(classes.false);
    expect(wrapper.find('div').text()).toEqual('false');
  });
});

describe('<JSONData/> test', () => {
  const classes = getClasses(
    <JSONData
      data={{
        testString: 'a',
        testNum: 10,
        testNull: null,
        testJSON: {
          hello: 'world',
        },
      }}
    />,
  );

  it('should render correctly for single line', () => {
    const wrapper = mount(
      <JSONData
        data={{
          testString: 'a',
          testNum: 10,
          testNull: null,
          testJSON: {
            hello: 'world',
          },
        }}
      />,
    );

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual(classes.base);
    const topLevelJSONContents = base.children();
    expect(topLevelJSONContents).toHaveLength(6);
    expect(topLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(5);
    expect(topLevelJSONContents.find({ className: classes.number })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.null })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.string })).toHaveLength(2);
    expect(topLevelJSONContents.find({ className: classes.base })).toHaveLength(1);

    const innerLevelJSONContents = topLevelJSONContents.find({
      className: classes.base,
    }).at(0).children();
    expect(innerLevelJSONContents.find({ className: classes.string })).toHaveLength(1);
    expect(innerLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(1);
  });

  it('should render correctly for multiline', () => {
    const wrapper = mount(
      <JSONData
        data={{
          testString: 'a',
          testNum: 10,
          testNull: null,
          testJSON: {
            hello: 'world',
          },
        }}
        multiline
      />,
    );

    expect(wrapper.text()).toEqual('{ testString: a, testNum: 10, testNull: null, testJSON: { hello: world } }');
    const base = wrapper.find('span').at(0);
    expect(base.prop('className')).toEqual(classes.base);
    const topLevelJSONContents = base.children();

    expect(topLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(5);
    expect(topLevelJSONContents.find({ className: classes.number })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.null })).toHaveLength(1);
    expect(topLevelJSONContents.find({ className: classes.string })).toHaveLength(2);
    expect(topLevelJSONContents.find({ className: classes.base })).toHaveLength(1);
    expect(topLevelJSONContents.find('br')).toHaveLength(7);

    const innerLevelJSONContents = topLevelJSONContents.find({
      className: classes.base,
    }).at(0).children();
    expect(innerLevelJSONContents.find({ className: classes.string })).toHaveLength(1);
    expect(innerLevelJSONContents.find({ className: classes.jsonKey })).toHaveLength(1);

    expect(wrapper.find(JSONData).at(1).props().multiline).toEqual(true);
    expect(wrapper.find(JSONData).at(1).props().indentation).toEqual(1);
  });

  it('should render array correctly for single line', () => {
    const wrapper = mount(
      <JSONData
        data={['some text', 'some other text']}
      />,
    );

    expect(wrapper.text()).toEqual('[ some text, some other text ]');
    expect(wrapper.find('br')).toHaveLength(0);
  });

  it('should render array correctly for multiline', () => {
    const wrapper = mount(
      <JSONData
        data={[
          { a: 1, b: { c: 'foo' } },
          { a: 3, b: null },
        ]}
        multiline
      />,
    );

    expect(wrapper.text()).toEqual('[ { a: 1, b: { c: foo } }, { a: 3, b: null } ]');
    expect(wrapper.find('br')).toHaveLength(11);
  });
});
